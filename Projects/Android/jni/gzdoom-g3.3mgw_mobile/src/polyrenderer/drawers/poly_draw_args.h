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

#include "r_data/r_translate.h"
#include "r_data/colormaps.h"
#include "screen_triangle.h"

class PolyRenderThread;
class FTexture;
class Mat4f;

enum class PolyDrawMode
{
	Triangles,
	TriangleFan,
	TriangleStrip
};

class PolyClipPlane
{
public:
	PolyClipPlane() : A(0.0f), B(0.0f), C(0.0f), D(1.0f) { }
	PolyClipPlane(float a, float b, float c, float d) : A(a), B(b), C(c), D(d) { }

	float A, B, C, D;
};

struct TriVertex
{
	TriVertex() { }
	TriVertex(float x, float y, float z, float w, float u, float v) : x(x), y(y), z(z), w(w), u(u), v(v) { }

	float x, y, z, w;
	float u, v;
};

struct PolyLight
{
	uint32_t color;
	float x, y, z;
	float radius;
};

class PolyDrawArgs
{
public:
	void SetClipPlane(int index, const PolyClipPlane &plane) { mClipPlane[index] = plane; }
	void SetTexture(const uint8_t *texels, int width, int height);
	void SetTexture(FTexture *texture, FRenderStyle style);
	void SetTexture(FTexture *texture, uint32_t translationID, FRenderStyle style);
	void SetLight(FSWColormap *basecolormap, uint32_t lightlevel, double globVis, bool fixed);
	void SetDepthTest(bool enable) { mDepthTest = enable; }
	void SetStencilTestValue(uint8_t stencilTestValue) { mStencilTestValue = stencilTestValue; }
	void SetWriteColor(bool enable) { mWriteColor = enable; }
	void SetWriteStencil(bool enable, uint8_t stencilWriteValue = 0) { mWriteStencil = enable; mStencilWriteValue = stencilWriteValue; }
	void SetWriteDepth(bool enable) { mWriteDepth = enable; }
	void SetStyle(TriBlendMode blendmode, double alpha = 1.0) { mBlendMode = blendmode; mAlpha = (uint32_t)(alpha * 256.0 + 0.5); }
	void SetStyle(const FRenderStyle &renderstyle, double alpha, uint32_t fillcolor, uint32_t translationID, FTexture *texture, bool fullbright);
	void SetColor(uint32_t bgra, uint8_t palindex);
	void SetLights(PolyLight *lights, int numLights) { mLights = lights; mNumLights = numLights; }
	void SetDynLightColor(uint32_t color) { mDynLightColor = color; }

	const PolyClipPlane &ClipPlane(int index) const { return mClipPlane[index]; }

	bool WriteColor() const { return mWriteColor; }

	FTexture *Texture() const { return mTexture; }
	const uint8_t *TexturePixels() const { return mTexturePixels; }
	int TextureWidth() const { return mTextureWidth; }
	int TextureHeight() const { return mTextureHeight; }
	const uint8_t *Translation() const { return mTranslation; }

	bool WriteStencil() const { return mWriteStencil; }
	uint8_t StencilTestValue() const { return mStencilTestValue; }
	uint8_t StencilWriteValue() const { return mStencilWriteValue; }

	bool DepthTest() const { return mDepthTest; }
	bool WriteDepth() const { return mWriteDepth; }

	TriBlendMode BlendMode() const { return mBlendMode; }
	uint32_t Color() const { return mColor; }
	uint32_t Alpha() const { return mAlpha; }

	float GlobVis() const { return mGlobVis; }
	uint32_t Light() const { return mLight; }
	const uint8_t *BaseColormap() const { return mColormaps; }
	uint16_t ShadeLightAlpha() const { return mLightAlpha; }
	uint16_t ShadeLightRed() const { return mLightRed; }
	uint16_t ShadeLightGreen() const { return mLightGreen; }
	uint16_t ShadeLightBlue() const { return mLightBlue; }
	uint16_t ShadeFadeAlpha() const { return mFadeAlpha; }
	uint16_t ShadeFadeRed() const { return mFadeRed; }
	uint16_t ShadeFadeGreen() const { return mFadeGreen; }
	uint16_t ShadeFadeBlue() const { return mFadeBlue; }
	uint16_t ShadeDesaturate() const { return mDesaturate; }

	bool SimpleShade() const { return mSimpleShade; }
	bool NearestFilter() const { return mNearestFilter; }
	bool FixedLight() const { return mFixedLight; }

	PolyLight *Lights() const { return mLights; }
	int NumLights() const { return mNumLights; }
	uint32_t DynLightColor() const { return mDynLightColor; }

	const FVector3 &Normal() const { return mNormal; }
	void SetNormal(const FVector3 &normal) { mNormal = normal; }

private:
	bool mDepthTest = false;
	bool mWriteStencil = true;
	bool mWriteColor = true;
	bool mWriteDepth = true;
	FTexture *mTexture = nullptr;
	const uint8_t *mTexturePixels = nullptr;
	int mTextureWidth = 0;
	int mTextureHeight = 0;
	const uint8_t *mTranslation = nullptr;
	uint8_t mStencilTestValue = 0;
	uint8_t mStencilWriteValue = 0;
	const uint8_t *mColormaps = nullptr;
	PolyClipPlane mClipPlane[3];
	TriBlendMode mBlendMode = TriBlendMode::Fill;
	uint32_t mLight = 0;
	uint32_t mColor = 0;
	uint32_t mAlpha = 0;
	uint16_t mLightAlpha = 0;
	uint16_t mLightRed = 0;
	uint16_t mLightGreen = 0;
	uint16_t mLightBlue = 0;
	uint16_t mFadeAlpha = 0;
	uint16_t mFadeRed = 0;
	uint16_t mFadeGreen = 0;
	uint16_t mFadeBlue = 0;
	uint16_t mDesaturate = 0;
	float mGlobVis = 0.0f;
	bool mSimpleShade = true;
	bool mNearestFilter = true;
	bool mFixedLight = false;
	PolyLight *mLights = nullptr;
	int mNumLights = 0;
	FVector3 mNormal;
	uint32_t mDynLightColor = 0;
};

class RectDrawArgs
{
public:
	void SetTexture(FTexture *texture, FRenderStyle style);
	void SetTexture(FTexture *texture, uint32_t translationID, FRenderStyle style);
	void SetLight(FSWColormap *basecolormap, uint32_t lightlevel);
	void SetStyle(TriBlendMode blendmode, double alpha = 1.0) { mBlendMode = blendmode; mAlpha = (uint32_t)(alpha * 256.0 + 0.5); }
	void SetStyle(const FRenderStyle &renderstyle, double alpha, uint32_t fillcolor, uint32_t translationID, FTexture *texture, bool fullbright);
	void SetColor(uint32_t bgra, uint8_t palindex);
	void Draw(PolyRenderThread *thread, double x0, double x1, double y0, double y1, double u0, double u1, double v0, double v1);

	FTexture *Texture() const { return mTexture; }
	const uint8_t *TexturePixels() const { return mTexturePixels; }
	int TextureWidth() const { return mTextureWidth; }
	int TextureHeight() const { return mTextureHeight; }
	const uint8_t *Translation() const { return mTranslation; }

	TriBlendMode BlendMode() const { return mBlendMode; }
	uint32_t Color() const { return mColor; }
	uint32_t Alpha() const { return mAlpha; }

	uint32_t Light() const { return mLight; }
	const uint8_t *BaseColormap() const { return mColormaps; }
	uint16_t ShadeLightAlpha() const { return mLightAlpha; }
	uint16_t ShadeLightRed() const { return mLightRed; }
	uint16_t ShadeLightGreen() const { return mLightGreen; }
	uint16_t ShadeLightBlue() const { return mLightBlue; }
	uint16_t ShadeFadeAlpha() const { return mFadeAlpha; }
	uint16_t ShadeFadeRed() const { return mFadeRed; }
	uint16_t ShadeFadeGreen() const { return mFadeGreen; }
	uint16_t ShadeFadeBlue() const { return mFadeBlue; }
	uint16_t ShadeDesaturate() const { return mDesaturate; }
	bool SimpleShade() const { return mSimpleShade; }

	float X0() const { return mX0; }
	float X1() const { return mX1; }
	float Y0() const { return mY0; }
	float Y1() const { return mY1; }
	float U0() const { return mU0; }
	float U1() const { return mU1; }
	float V0() const { return mV0; }
	float V1() const { return mV1; }

private:
	FTexture *mTexture = nullptr;
	const uint8_t *mTexturePixels = nullptr;
	int mTextureWidth = 0;
	int mTextureHeight = 0;
	const uint8_t *mTranslation = nullptr;
	const uint8_t *mColormaps = nullptr;
	TriBlendMode mBlendMode = TriBlendMode::Fill;
	uint32_t mLight = 0;
	uint32_t mColor = 0;
	uint32_t mAlpha = 0;
	uint16_t mLightAlpha = 0;
	uint16_t mLightRed = 0;
	uint16_t mLightGreen = 0;
	uint16_t mLightBlue = 0;
	uint16_t mFadeAlpha = 0;
	uint16_t mFadeRed = 0;
	uint16_t mFadeGreen = 0;
	uint16_t mFadeBlue = 0;
	uint16_t mDesaturate = 0;
	bool mSimpleShade = true;
	float mX0, mX1, mY0, mY1, mU0, mU1, mV0, mV1;
};
