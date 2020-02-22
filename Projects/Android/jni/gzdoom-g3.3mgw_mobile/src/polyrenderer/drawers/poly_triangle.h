/*
**  Polygon Doom software renderer
**  Copyright (c) 2016 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#pragma once

#include "swrenderer/drawers/r_draw.h"
#include "swrenderer/drawers/r_thread.h"
#include "polyrenderer/drawers/screen_triangle.h"
#include "polyrenderer/math/gpu_types.h"
#include "polyrenderer/drawers/poly_buffer.h"
#include "polyrenderer/drawers/poly_draw_args.h"

class PolyTriangleDrawer
{
public:
	static void ClearBuffers(DCanvas *canvas);
	static void SetViewport(const DrawerCommandQueuePtr &queue, int x, int y, int width, int height, DCanvas *canvas, bool span_drawers);
	static void SetCullCCW(const DrawerCommandQueuePtr &queue, bool ccw);
	static void SetTwoSided(const DrawerCommandQueuePtr &queue, bool twosided);
	static void SetWeaponScene(const DrawerCommandQueuePtr &queue, bool enable);
	static void SetModelVertexShader(const DrawerCommandQueuePtr &queue, int frame1, int frame2, float interpolationFactor);
	static void SetTransform(const DrawerCommandQueuePtr &queue, const Mat4f *objectToClip, const Mat4f *objectToWorld);
	static void DrawArray(const DrawerCommandQueuePtr &queue, const PolyDrawArgs &args, const void *vertices, int vcount, PolyDrawMode mode = PolyDrawMode::Triangles);
	static void DrawElements(const DrawerCommandQueuePtr &queue, const PolyDrawArgs &args, const void *vertices, const unsigned int *elements, int count, PolyDrawMode mode = PolyDrawMode::Triangles);
	static bool IsBgra();
};

class PolyTriangleThreadData
{
public:
	PolyTriangleThreadData(int32_t core, int32_t num_cores) : core(core), num_cores(num_cores) { }

	void SetViewport(int x, int y, int width, int height, uint8_t *dest, int dest_width, int dest_height, int dest_pitch, bool dest_bgra, bool span_drawers);
	void SetTransform(const Mat4f *objectToClip, const Mat4f *objectToWorld);
	void SetCullCCW(bool value) { ccw = value; }
	void SetTwoSided(bool value) { twosided = value; }
	void SetWeaponScene(bool value) { weaponScene = value; }
	void SetModelVertexShader(int frame1, int frame2, float interpolationFactor) { modelFrame1 = frame1; modelFrame2 = frame2; modelInterpolationFactor = interpolationFactor; }

	void DrawElements(const PolyDrawArgs &args, const void *vertices, const unsigned int *elements, int count, PolyDrawMode mode);
	void DrawArray(const PolyDrawArgs &args, const void *vertices, int vcount, PolyDrawMode mode);

	int32_t core;
	int32_t num_cores;

	// The number of lines to skip to reach the first line to be rendered by this thread
	int skipped_by_thread(int first_line)
	{
		int core_skip = (num_cores - (first_line - core) % num_cores) % num_cores;
		return core_skip;
	}

	static PolyTriangleThreadData *Get(DrawerThread *thread);

private:
	ShadedTriVertex ShadeVertex(const PolyDrawArgs &drawargs, const void *vertices, int index);
	void DrawShadedTriangle(const ShadedTriVertex *vertices, bool ccw, TriDrawTriangleArgs *args);
	static bool IsDegenerate(const ShadedTriVertex *vertices);
	static bool IsFrontfacing(TriDrawTriangleArgs *args);
	static int ClipEdge(const ShadedTriVertex *verts, ShadedTriVertex *clippedvert);

	int viewport_x = 0;
	int viewport_y = 0;
	int viewport_width = 0;
	int viewport_height = 0;
	int dest_pitch = 0;
	int dest_width = 0;
	int dest_height = 0;
	bool dest_bgra = false;
	uint8_t *dest = nullptr;
	bool ccw = true;
	bool twosided = false;
	bool weaponScene = false;
	const Mat4f *objectToClip = nullptr;
	const Mat4f *objectToWorld = nullptr;
	int modelFrame1 = -1;
	int modelFrame2 = -1;
	float modelInterpolationFactor = 0.0f;
	bool span_drawers = false;

	enum { max_additional_vertices = 16 };
};

class PolySetTransformCommand : public DrawerCommand
{
public:
	PolySetTransformCommand(const Mat4f *objectToClip, const Mat4f *objectToWorld);

	void Execute(DrawerThread *thread) override;

private:
	const Mat4f *objectToClip;
	const Mat4f *objectToWorld;
};

class PolySetCullCCWCommand : public DrawerCommand
{
public:
	PolySetCullCCWCommand(bool ccw);

	void Execute(DrawerThread *thread) override;

private:
	bool ccw;
};

class PolySetTwoSidedCommand : public DrawerCommand
{
public:
	PolySetTwoSidedCommand(bool twosided);

	void Execute(DrawerThread *thread) override;

private:
	bool twosided;
};

class PolySetWeaponSceneCommand : public DrawerCommand
{
public:
	PolySetWeaponSceneCommand(bool value);

	void Execute(DrawerThread *thread) override;

private:
	bool value;
};

class PolySetModelVertexShaderCommand : public DrawerCommand
{
public:
	PolySetModelVertexShaderCommand(int frame1, int frame2, float interpolationFactor);

	void Execute(DrawerThread *thread) override;

private:
	int frame1;
	int frame2;
	float interpolationFactor;
};

class PolySetViewportCommand : public DrawerCommand
{
public:
	PolySetViewportCommand(int x, int y, int width, int height, uint8_t *dest, int dest_width, int dest_height, int dest_pitch, bool dest_bgra, bool span_drawers);

	void Execute(DrawerThread *thread) override;

private:
	int x;
	int y;
	int width;
	int height;
	uint8_t *dest;
	int dest_width;
	int dest_height;
	int dest_pitch;
	bool dest_bgra;
	bool span_drawers;
};

class DrawPolyTrianglesCommand : public DrawerCommand
{
public:
	DrawPolyTrianglesCommand(const PolyDrawArgs &args, const void *vertices, const unsigned int *elements, int count, PolyDrawMode mode);

	void Execute(DrawerThread *thread) override;

private:
	PolyDrawArgs args;
	const void *vertices;
	const unsigned int *elements;
	int count;
	PolyDrawMode mode;
};

class DrawRectCommand : public DrawerCommand
{
public:
	DrawRectCommand(const RectDrawArgs &args) : args(args) { }

	void Execute(DrawerThread *thread) override;

private:
	RectDrawArgs args;
};
