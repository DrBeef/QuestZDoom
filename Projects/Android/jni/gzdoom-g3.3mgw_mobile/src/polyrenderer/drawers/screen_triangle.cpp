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

#include <stddef.h>
#include "templates.h"
#include "doomdef.h"
#include "i_system.h"
#include "w_wad.h"
#include "v_video.h"
#include "doomstat.h"
#include "st_stuff.h"
#include "g_game.h"
#include "g_level.h"
#include "r_data/r_translate.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "poly_triangle.h"
#include "swrenderer/drawers/r_draw_rgba.h"
#include "screen_triangle.h"
#include "x86.h"

class TriangleBlock
{
public:
	TriangleBlock(const TriDrawTriangleArgs *args, PolyTriangleThreadData *thread);
	void Render();

private:
	void RenderSubdivide(int x0, int y0, int x1, int y1);

	enum class CoverageModes { Full, Partial };
	struct CoverageFull { static const int Mode = (int)CoverageModes::Full; };
	struct CoveragePartial { static const int Mode = (int)CoverageModes::Partial; };

	template<typename CoverageMode>
	void RenderBlock(int x0, int y0, int x1, int y1);

	const TriDrawTriangleArgs *args;
	PolyTriangleThreadData *thread;

	// Block size, standard 8x8 (must be power of two)
	static const int q = 8;

	// Deltas
	int DX12, DX23, DX31;
	int DY12, DY23, DY31;

	// Fixed-point deltas
	int FDX12, FDX23, FDX31;
	int FDY12, FDY23, FDY31;

	// Half-edge constants
	int C1, C2, C3;

	// Stencil buffer
	int stencilPitch;
	uint8_t * RESTRICT stencilValues;
	uint32_t * RESTRICT stencilMasks;
	uint8_t stencilTestValue;
	uint32_t stencilWriteValue;

	// Viewport clipping
	int clipright;
	int clipbottom;

	// Depth buffer
	float * RESTRICT zbuffer;
	int32_t zbufferPitch;

	// Triangle bounding block
	int minx, miny;
	int maxx, maxy;

	// Active block
	int X, Y;
	uint32_t Mask0, Mask1;

#ifndef NO_SSE
	__m128i mFDY12Offset;
	__m128i mFDY23Offset;
	__m128i mFDY31Offset;
	__m128i mFDY12x4;
	__m128i mFDY23x4;
	__m128i mFDY31x4;
	__m128i mFDX12;
	__m128i mFDX23;
	__m128i mFDX31;
	__m128i mC1;
	__m128i mC2;
	__m128i mC3;
	__m128i mDX12;
	__m128i mDY12;
	__m128i mDX23;
	__m128i mDY23;
	__m128i mDX31;
	__m128i mDY31;
#endif

	enum class CoverageResult
	{
		full,
		partial,
		none
	};
	CoverageResult AreaCoverageTest(int x0, int y0, int x1, int y1);

	void CoverageTest();
	void StencilEqualTest();
	void StencilGreaterEqualTest();
	void DepthTest(const TriDrawTriangleArgs *args);
	void ClipTest();
	void StencilWrite();
	void DepthWrite(const TriDrawTriangleArgs *args);
};

TriangleBlock::TriangleBlock(const TriDrawTriangleArgs *args, PolyTriangleThreadData *thread) : args(args), thread(thread)
{
	const ShadedTriVertex &v1 = *args->v1;
	const ShadedTriVertex &v2 = *args->v2;
	const ShadedTriVertex &v3 = *args->v3;

	clipright = args->clipright;
	clipbottom = args->clipbottom;

	stencilPitch = args->stencilPitch;
	stencilValues = args->stencilValues;
	stencilMasks = args->stencilMasks;
	stencilTestValue = args->uniforms->StencilTestValue();
	stencilWriteValue = args->uniforms->StencilWriteValue();

	zbuffer = args->zbuffer;
	zbufferPitch = args->stencilPitch;

	// 28.4 fixed-point coordinates
#ifdef NO_SSE
	const int Y1 = (int)round(16.0f * v1.y);
	const int Y2 = (int)round(16.0f * v2.y);
	const int Y3 = (int)round(16.0f * v3.y);

	const int X1 = (int)round(16.0f * v1.x);
	const int X2 = (int)round(16.0f * v2.x);
	const int X3 = (int)round(16.0f * v3.x);
#else
	int tempround[4 * 3];
	__m128 m16 = _mm_set1_ps(16.0f);
	__m128 mhalf = _mm_set1_ps(65536.5f);
	__m128i m65536 = _mm_set1_epi32(65536);
	_mm_storeu_si128((__m128i*)tempround, _mm_sub_epi32(_mm_cvtps_epi32(_mm_add_ps(_mm_mul_ps(_mm_loadu_ps((const float*)&v1), m16), mhalf)), m65536));
	_mm_storeu_si128((__m128i*)(tempround + 4), _mm_sub_epi32(_mm_cvtps_epi32(_mm_add_ps(_mm_mul_ps(_mm_loadu_ps((const float*)&v2), m16), mhalf)), m65536));
	_mm_storeu_si128((__m128i*)(tempround + 8), _mm_sub_epi32(_mm_cvtps_epi32(_mm_add_ps(_mm_mul_ps(_mm_loadu_ps((const float*)&v3), m16), mhalf)), m65536));
	const int X1 = tempround[0];
	const int X2 = tempround[4];
	const int X3 = tempround[8];
	const int Y1 = tempround[1];
	const int Y2 = tempround[5];
	const int Y3 = tempround[9];
#endif

	// Deltas
	DX12 = X1 - X2;
	DX23 = X2 - X3;
	DX31 = X3 - X1;

	DY12 = Y1 - Y2;
	DY23 = Y2 - Y3;
	DY31 = Y3 - Y1;

	// Fixed-point deltas
	FDX12 = DX12 << 4;
	FDX23 = DX23 << 4;
	FDX31 = DX31 << 4;

	FDY12 = DY12 << 4;
	FDY23 = DY23 << 4;
	FDY31 = DY31 << 4;

	// Bounding rectangle
	minx = MAX((MIN(MIN(X1, X2), X3) + 0xF) >> 4, 0);
	maxx = MIN((MAX(MAX(X1, X2), X3) + 0xF) >> 4, clipright - 1);
	miny = MAX((MIN(MIN(Y1, Y2), Y3) + 0xF) >> 4, 0);
	maxy = MIN((MAX(MAX(Y1, Y2), Y3) + 0xF) >> 4, clipbottom - 1);
	if (minx >= maxx || miny >= maxy)
	{
		return;
	}

	// Start and end in corner of 8x8 block
	minx &= ~(q - 1);
	miny &= ~(q - 1);
	maxx |= q - 1;
	maxy |= q - 1;

	// Half-edge constants
	C1 = DY12 * X1 - DX12 * Y1;
	C2 = DY23 * X2 - DX23 * Y2;
	C3 = DY31 * X3 - DX31 * Y3;

	// Correct for fill convention
	if (DY12 < 0 || (DY12 == 0 && DX12 > 0)) C1++;
	if (DY23 < 0 || (DY23 == 0 && DX23 > 0)) C2++;
	if (DY31 < 0 || (DY31 == 0 && DX31 > 0)) C3++;

#ifndef NO_SSE
	mFDY12Offset = _mm_setr_epi32(0, FDY12, FDY12 * 2, FDY12 * 3);
	mFDY23Offset = _mm_setr_epi32(0, FDY23, FDY23 * 2, FDY23 * 3);
	mFDY31Offset = _mm_setr_epi32(0, FDY31, FDY31 * 2, FDY31 * 3);
	mFDY12x4 = _mm_set1_epi32(FDY12 * 4);
	mFDY23x4 = _mm_set1_epi32(FDY23 * 4);
	mFDY31x4 = _mm_set1_epi32(FDY31 * 4);
	mFDX12 = _mm_set1_epi32(FDX12);
	mFDX23 = _mm_set1_epi32(FDX23);
	mFDX31 = _mm_set1_epi32(FDX31);
	mC1 = _mm_set1_epi32(C1);
	mC2 = _mm_set1_epi32(C2);
	mC3 = _mm_set1_epi32(C3);
	mDX12 = _mm_set1_epi32(DX12);
	mDY12 = _mm_set1_epi32(DY12);
	mDX23 = _mm_set1_epi32(DX23);
	mDY23 = _mm_set1_epi32(DY23);
	mDX31 = _mm_set1_epi32(DX31);
	mDY31 = _mm_set1_epi32(DY31);
#endif
}

void TriangleBlock::Render()
{
	RenderSubdivide(minx / q, miny / q, (maxx + 1) / q, (maxy + 1) / q);
}

void TriangleBlock::RenderSubdivide(int x0, int y0, int x1, int y1)
{
	CoverageResult result = AreaCoverageTest(x0 * q, y0 * q, x1 * q, y1 * q);
	if (result == CoverageResult::full)
	{
		RenderBlock<CoverageFull>(x0 * q, y0 * q, x1 * q, y1 * q);
	}
	else if (result == CoverageResult::partial)
	{
		bool doneX = x1 - x0 <= 8;
		bool doneY = y1 - y0 <= 8;
		if (doneX && doneY)
		{
			RenderBlock<CoveragePartial>(x0 * q, y0 * q, x1 * q, y1 * q);
		}
		else
		{
			int midx = (x0 + x1) >> 1;
			int midy = (y0 + y1) >> 1;
			if (doneX)
			{
				RenderSubdivide(x0, y0, x1, midy);
				RenderSubdivide(x0, midy, x1, y1);
			}
			else if (doneY)
			{
				RenderSubdivide(x0, y0, midx, y1);
				RenderSubdivide(midx, y0, x1, y1);
			}
			else
			{
				RenderSubdivide(x0, y0, midx, midy);
				RenderSubdivide(midx, y0, x1, midy);
				RenderSubdivide(x0, midy, midx, y1);
				RenderSubdivide(midx, midy, x1, y1);
			}
		}
	}
}

template<typename CoverageModeT>
void TriangleBlock::RenderBlock(int x0, int y0, int x1, int y1)
{
	// First block line for this thread
	int core = thread->core;
	int num_cores = thread->num_cores;
	int core_skip = (num_cores - ((y0 / q) - core) % num_cores) % num_cores;
	int start_miny = y0 + core_skip * q;

	bool depthTest = args->uniforms->DepthTest();
	bool writeColor = args->uniforms->WriteColor();
	bool writeStencil = args->uniforms->WriteStencil();
	bool writeDepth = args->uniforms->WriteDepth();

	int bmode = (int)args->uniforms->BlendMode();
	auto drawFunc = args->destBgra ? ScreenTriangle::SpanDrawers32[bmode] : ScreenTriangle::SpanDrawers8[bmode];

	// Loop through blocks
	for (int y = start_miny; y < y1; y += q * num_cores)
	{
		for (int x = x0; x < x1; x += q)
		{
			X = x;
			Y = y;

			if (CoverageModeT::Mode == (int)CoverageModes::Full)
			{
				Mask0 = 0xffffffff;
				Mask1 = 0xffffffff;
			}
			else
			{
				CoverageTest();
				if (Mask0 == 0 && Mask1 == 0)
					continue;
			}

			ClipTest();
			if (Mask0 == 0 && Mask1 == 0)
				continue;

			StencilEqualTest();
			if (Mask0 == 0 && Mask1 == 0)
				continue;

			if (depthTest)
			{
				DepthTest(args);
				if (Mask0 == 0 && Mask1 == 0)
					continue;
			}

			if (writeColor)
			{
				if (Mask0 == 0xffffffff)
				{
					drawFunc(Y, X, X + 8, args);
					drawFunc(Y + 1, X, X + 8, args);
					drawFunc(Y + 2, X, X + 8, args);
					drawFunc(Y + 3, X, X + 8, args);
				}
				else if (Mask0 != 0)
				{
					uint32_t mask = Mask0;
					for (int j = 0; j < 4; j++)
					{
						int start = 0;
						int i;
						for (i = 0; i < 8; i++)
						{
							if (!(mask & 0x80000000))
							{
								if (i > start)
									drawFunc(Y + j, X + start, X + i, args);
								start = i + 1;
							}
							mask <<= 1;
						}
						if (i > start)
							drawFunc(Y + j, X + start, X + i, args);
					}
				}

				if (Mask1 == 0xffffffff)
				{
					drawFunc(Y + 4, X, X + 8, args);
					drawFunc(Y + 5, X, X + 8, args);
					drawFunc(Y + 6, X, X + 8, args);
					drawFunc(Y + 7, X, X + 8, args);
				}
				else if (Mask1 != 0)
				{
					uint32_t mask = Mask1;
					for (int j = 4; j < 8; j++)
					{
						int start = 0;
						int i;
						for (i = 0; i < 8; i++)
						{
							if (!(mask & 0x80000000))
							{
								if (i > start)
									drawFunc(Y + j, X + start, X + i, args);
								start = i + 1;
							}
							mask <<= 1;
						}
						if (i > start)
							drawFunc(Y + j, X + start, X + i, args);
					}
				}
			}

			if (writeStencil)
				StencilWrite();
			if (writeDepth)
				DepthWrite(args);
		}
	}
}

#ifdef NO_SSE

void TriangleBlock::DepthTest(const TriDrawTriangleArgs *args)
{
	int block = (X >> 3) + (Y >> 3) * zbufferPitch;
	float *depth = zbuffer + block * 64;

	const ShadedTriVertex &v1 = *args->v1;

	float stepXW = args->gradientX.W;
	float stepYW = args->gradientY.W;
	float posYW = v1.w + stepXW * (X - v1.x) + stepYW * (Y - v1.y) + args->depthOffset;

	uint32_t mask0 = 0;
	uint32_t mask1 = 0;

	for (int iy = 0; iy < 4; iy++)
	{
		float posXW = posYW;
		for (int ix = 0; ix < 8; ix++)
		{
			bool covered = *depth <= posXW;
			mask0 <<= 1;
			mask0 |= (uint32_t)covered;
			depth++;
			posXW += stepXW;
		}
		posYW += stepYW;
	}

	for (int iy = 0; iy < 4; iy++)
	{
		float posXW = posYW;
		for (int ix = 0; ix < 8; ix++)
		{
			bool covered = *depth <= posXW;
			mask1 <<= 1;
			mask1 |= (uint32_t)covered;
			depth++;
			posXW += stepXW;
		}
		posYW += stepYW;
	}

	Mask0 = Mask0 & mask0;
	Mask1 = Mask1 & mask1;
}

#else

void TriangleBlock::DepthTest(const TriDrawTriangleArgs *args)
{
	int block = (X >> 3) + (Y >> 3) * zbufferPitch;
	float *depth = zbuffer + block * 64;

	const ShadedTriVertex &v1 = *args->v1;

	float stepXW = args->gradientX.W;
	float stepYW = args->gradientY.W;
	float posYW = v1.w + stepXW * (X - v1.x) + stepYW * (Y - v1.y) + args->depthOffset;

	__m128 mposYW = _mm_setr_ps(posYW, posYW + stepXW, posYW + stepXW + stepXW, posYW + stepXW + stepXW + stepXW);
	__m128 mstepXW = _mm_set1_ps(stepXW * 4.0f);
	__m128 mstepYW = _mm_set1_ps(stepYW);

	uint32_t mask0 = 0;
	uint32_t mask1 = 0;

	for (int iy = 0; iy < 4; iy++)
	{
		__m128 mposXW = mposYW;
		for (int ix = 0; ix < 2; ix++)
		{
			__m128 covered = _mm_cmplt_ps(_mm_loadu_ps(depth), mposXW);
			mask0 <<= 4;
			mask0 |= _mm_movemask_ps(_mm_shuffle_ps(covered, covered, _MM_SHUFFLE(0, 1, 2, 3)));
			depth += 4;
			mposXW = _mm_add_ps(mposXW, mstepXW);
		}
		mposYW = _mm_add_ps(mposYW, mstepYW);
	}

	for (int iy = 0; iy < 4; iy++)
	{
		__m128 mposXW = mposYW;
		for (int ix = 0; ix < 2; ix++)
		{
			__m128 covered = _mm_cmplt_ps(_mm_loadu_ps(depth), mposXW);
			mask1 <<= 4;
			mask1 |= _mm_movemask_ps(_mm_shuffle_ps(covered, covered, _MM_SHUFFLE(0, 1, 2, 3)));
			depth += 4;
			mposXW = _mm_add_ps(mposXW, mstepXW);
		}
		mposYW = _mm_add_ps(mposYW, mstepYW);
	}

	Mask0 = Mask0 & mask0;
	Mask1 = Mask1 & mask1;
}

#endif

void TriangleBlock::ClipTest()
{
	static const uint32_t clipxmask[8] =
	{
		0,
		0x80808080,
		0xc0c0c0c0,
		0xe0e0e0e0,
		0xf0f0f0f0,
		0xf8f8f8f8,
		0xfcfcfcfc,
		0xfefefefe
	};

	static const uint32_t clipymask[8] =
	{
		0,
		0xff000000,
		0xffff0000,
		0xffffff00,
		0xffffffff,
		0xffffffff,
		0xffffffff,
		0xffffffff
	};

	uint32_t xmask = (X + 8 <= clipright) ? 0xffffffff : clipxmask[clipright - X];
	uint32_t ymask0 = (Y + 4 <= clipbottom) ? 0xffffffff : clipymask[clipbottom - Y];
	uint32_t ymask1 = (Y + 8 <= clipbottom) ? 0xffffffff : clipymask[clipbottom - Y - 4];

	Mask0 = Mask0 & xmask & ymask0;
	Mask1 = Mask1 & xmask & ymask1;
}

#ifdef NO_SSE

void TriangleBlock::StencilEqualTest()
{
	// Stencil test the whole block, if possible
	int block = (X >> 3) + (Y >> 3) * stencilPitch;
	uint8_t *stencilBlock = &stencilValues[block * 64];
	uint32_t *stencilBlockMask = &stencilMasks[block];
	bool blockIsSingleStencil = ((*stencilBlockMask) & 0xffffff00) == 0xffffff00;
	bool skipBlock = blockIsSingleStencil && ((*stencilBlockMask) & 0xff) != stencilTestValue;
	if (skipBlock)
	{
		Mask0 = 0;
		Mask1 = 0;
	}
	else if (!blockIsSingleStencil)
	{
		uint32_t mask0 = 0;
		uint32_t mask1 = 0;

		for (int iy = 0; iy < 4; iy++)
		{
			for (int ix = 0; ix < q; ix++)
			{
				bool passStencilTest = stencilBlock[ix + iy * q] == stencilTestValue;
				mask0 <<= 1;
				mask0 |= (uint32_t)passStencilTest;
			}
		}

		for (int iy = 4; iy < q; iy++)
		{
			for (int ix = 0; ix < q; ix++)
			{
				bool passStencilTest = stencilBlock[ix + iy * q] == stencilTestValue;
				mask1 <<= 1;
				mask1 |= (uint32_t)passStencilTest;
			}
		}

		Mask0 = Mask0 & mask0;
		Mask1 = Mask1 & mask1;
	}
}

#else

void TriangleBlock::StencilEqualTest()
{
	// Stencil test the whole block, if possible
	int block = (X >> 3) + (Y >> 3) * stencilPitch;
	uint8_t *stencilBlock = &stencilValues[block * 64];
	uint32_t *stencilBlockMask = &stencilMasks[block];
	bool blockIsSingleStencil = ((*stencilBlockMask) & 0xffffff00) == 0xffffff00;
	bool skipBlock = blockIsSingleStencil && ((*stencilBlockMask) & 0xff) != stencilTestValue;
	if (skipBlock)
	{
		Mask0 = 0;
		Mask1 = 0;
	}
	else if (!blockIsSingleStencil)
	{
		__m128i mstencilTestValue = _mm_set1_epi16(stencilTestValue);
		uint32_t mask0 = 0;
		uint32_t mask1 = 0;

		for (int iy = 0; iy < 2; iy++)
		{
			__m128i mstencilBlock = _mm_loadu_si128((const __m128i *)stencilBlock);

			__m128i mstencilTest = _mm_cmpeq_epi16(_mm_unpacklo_epi8(mstencilBlock, _mm_setzero_si128()), mstencilTestValue);
			__m128i mstencilTest0 = _mm_unpacklo_epi16(mstencilTest, mstencilTest);
			__m128i mstencilTest1 = _mm_unpackhi_epi16(mstencilTest, mstencilTest);
			__m128i first = _mm_packs_epi32(_mm_shuffle_epi32(mstencilTest1, _MM_SHUFFLE(0, 1, 2, 3)), _mm_shuffle_epi32(mstencilTest0, _MM_SHUFFLE(0, 1, 2, 3)));

			mstencilTest = _mm_cmpeq_epi16(_mm_unpackhi_epi8(mstencilBlock, _mm_setzero_si128()), mstencilTestValue);
			mstencilTest0 = _mm_unpacklo_epi16(mstencilTest, mstencilTest);
			mstencilTest1 = _mm_unpackhi_epi16(mstencilTest, mstencilTest);
			__m128i second = _mm_packs_epi32(_mm_shuffle_epi32(mstencilTest1, _MM_SHUFFLE(0, 1, 2, 3)), _mm_shuffle_epi32(mstencilTest0, _MM_SHUFFLE(0, 1, 2, 3)));

			mask0 <<= 16;
			mask0 |= _mm_movemask_epi8(_mm_packs_epi16(second, first));

			stencilBlock += 16;
		}

		for (int iy = 0; iy < 2; iy++)
		{
			__m128i mstencilBlock = _mm_loadu_si128((const __m128i *)stencilBlock);

			__m128i mstencilTest = _mm_cmpeq_epi16(_mm_unpacklo_epi8(mstencilBlock, _mm_setzero_si128()), mstencilTestValue);
			__m128i mstencilTest0 = _mm_unpacklo_epi16(mstencilTest, mstencilTest);
			__m128i mstencilTest1 = _mm_unpackhi_epi16(mstencilTest, mstencilTest);
			__m128i first = _mm_packs_epi32(_mm_shuffle_epi32(mstencilTest1, _MM_SHUFFLE(0, 1, 2, 3)), _mm_shuffle_epi32(mstencilTest0, _MM_SHUFFLE(0, 1, 2, 3)));

			mstencilTest = _mm_cmpeq_epi16(_mm_unpackhi_epi8(mstencilBlock, _mm_setzero_si128()), mstencilTestValue);
			mstencilTest0 = _mm_unpacklo_epi16(mstencilTest, mstencilTest);
			mstencilTest1 = _mm_unpackhi_epi16(mstencilTest, mstencilTest);
			__m128i second = _mm_packs_epi32(_mm_shuffle_epi32(mstencilTest1, _MM_SHUFFLE(0, 1, 2, 3)), _mm_shuffle_epi32(mstencilTest0, _MM_SHUFFLE(0, 1, 2, 3)));

			mask1 <<= 16;
			mask1 |= _mm_movemask_epi8(_mm_packs_epi16(second, first));

			stencilBlock += 16;
		}

		Mask0 = Mask0 & mask0;
		Mask1 = Mask1 & mask1;
	}
}

#endif

void TriangleBlock::StencilGreaterEqualTest()
{
	// Stencil test the whole block, if possible
	int block = (X >> 3) + (Y >> 3) * stencilPitch;
	uint8_t *stencilBlock = &stencilValues[block * 64];
	uint32_t *stencilBlockMask = &stencilMasks[block];
	bool blockIsSingleStencil = ((*stencilBlockMask) & 0xffffff00) == 0xffffff00;
	bool skipBlock = blockIsSingleStencil && ((*stencilBlockMask) & 0xff) < stencilTestValue;
	if (skipBlock)
	{
		Mask0 = 0;
		Mask1 = 0;
	}
	else if (!blockIsSingleStencil)
	{
		uint32_t mask0 = 0;
		uint32_t mask1 = 0;

		for (int iy = 0; iy < 4; iy++)
		{
			for (int ix = 0; ix < q; ix++)
			{
				bool passStencilTest = stencilBlock[ix + iy * q] >= stencilTestValue;
				mask0 <<= 1;
				mask0 |= (uint32_t)passStencilTest;
			}
		}

		for (int iy = 4; iy < q; iy++)
		{
			for (int ix = 0; ix < q; ix++)
			{
				bool passStencilTest = stencilBlock[ix + iy * q] >= stencilTestValue;
				mask1 <<= 1;
				mask1 |= (uint32_t)passStencilTest;
			}
		}

		Mask0 = Mask0 & mask0;
		Mask1 = Mask1 & mask1;
	}
}

TriangleBlock::CoverageResult TriangleBlock::AreaCoverageTest(int x0, int y0, int x1, int y1)
{
	// Corners of block
	x0 = x0 << 4;
	x1 = (x1 - 1) << 4;
	y0 = y0 << 4;
	y1 = (y1 - 1) << 4;

	// Evaluate half-space functions
	bool a00 = C1 + DX12 * y0 - DY12 * x0 > 0;
	bool a10 = C1 + DX12 * y0 - DY12 * x1 > 0;
	bool a01 = C1 + DX12 * y1 - DY12 * x0 > 0;
	bool a11 = C1 + DX12 * y1 - DY12 * x1 > 0;
	int a = (a00 << 0) | (a10 << 1) | (a01 << 2) | (a11 << 3);

	bool b00 = C2 + DX23 * y0 - DY23 * x0 > 0;
	bool b10 = C2 + DX23 * y0 - DY23 * x1 > 0;
	bool b01 = C2 + DX23 * y1 - DY23 * x0 > 0;
	bool b11 = C2 + DX23 * y1 - DY23 * x1 > 0;
	int b = (b00 << 0) | (b10 << 1) | (b01 << 2) | (b11 << 3);

	bool c00 = C3 + DX31 * y0 - DY31 * x0 > 0;
	bool c10 = C3 + DX31 * y0 - DY31 * x1 > 0;
	bool c01 = C3 + DX31 * y1 - DY31 * x0 > 0;
	bool c11 = C3 + DX31 * y1 - DY31 * x1 > 0;
	int c = (c00 << 0) | (c10 << 1) | (c01 << 2) | (c11 << 3);

	if (a == 0 || b == 0 || c == 0) // Skip block when outside an edge
	{
		return CoverageResult::none;
	}
	else if (a == 0xf && b == 0xf && c == 0xf) // Accept whole block when totally covered
	{
		return CoverageResult::full;
	}
	else // Partially covered block
	{
		return CoverageResult::partial;
	}
}

#ifdef NO_SSE

void TriangleBlock::CoverageTest()
{
	// Corners of block
	int x0 = X << 4;
	int x1 = (X + q - 1) << 4;
	int y0 = Y << 4;
	int y1 = (Y + q - 1) << 4;

	// Evaluate half-space functions
	bool a00 = C1 + DX12 * y0 - DY12 * x0 > 0;
	bool a10 = C1 + DX12 * y0 - DY12 * x1 > 0;
	bool a01 = C1 + DX12 * y1 - DY12 * x0 > 0;
	bool a11 = C1 + DX12 * y1 - DY12 * x1 > 0;
	int a = (a00 << 0) | (a10 << 1) | (a01 << 2) | (a11 << 3);

	bool b00 = C2 + DX23 * y0 - DY23 * x0 > 0;
	bool b10 = C2 + DX23 * y0 - DY23 * x1 > 0;
	bool b01 = C2 + DX23 * y1 - DY23 * x0 > 0;
	bool b11 = C2 + DX23 * y1 - DY23 * x1 > 0;
	int b = (b00 << 0) | (b10 << 1) | (b01 << 2) | (b11 << 3);

	bool c00 = C3 + DX31 * y0 - DY31 * x0 > 0;
	bool c10 = C3 + DX31 * y0 - DY31 * x1 > 0;
	bool c01 = C3 + DX31 * y1 - DY31 * x0 > 0;
	bool c11 = C3 + DX31 * y1 - DY31 * x1 > 0;
	int c = (c00 << 0) | (c10 << 1) | (c01 << 2) | (c11 << 3);

	if (a == 0 || b == 0 || c == 0) // Skip block when outside an edge
	{
		Mask0 = 0;
		Mask1 = 0;
	}
	else if (a == 0xf && b == 0xf && c == 0xf) // Accept whole block when totally covered
	{
		Mask0 = 0xffffffff;
		Mask1 = 0xffffffff;
	}
	else // Partially covered block
	{
		x0 = X << 4;
		x1 = (X + q - 1) << 4;
		int CY1 = C1 + DX12 * y0 - DY12 * x0;
		int CY2 = C2 + DX23 * y0 - DY23 * x0;
		int CY3 = C3 + DX31 * y0 - DY31 * x0;

		uint32_t mask0 = 0;
		uint32_t mask1 = 0;

		for (int iy = 0; iy < 4; iy++)
		{
			int CX1 = CY1;
			int CX2 = CY2;
			int CX3 = CY3;

			for (int ix = 0; ix < q; ix++)
			{
				bool covered = CX1 > 0 && CX2 > 0 && CX3 > 0;
				mask0 <<= 1;
				mask0 |= (uint32_t)covered;

				CX1 -= FDY12;
				CX2 -= FDY23;
				CX3 -= FDY31;
			}

			CY1 += FDX12;
			CY2 += FDX23;
			CY3 += FDX31;
		}

		for (int iy = 4; iy < q; iy++)
		{
			int CX1 = CY1;
			int CX2 = CY2;
			int CX3 = CY3;

			for (int ix = 0; ix < q; ix++)
			{
				bool covered = CX1 > 0 && CX2 > 0 && CX3 > 0;
				mask1 <<= 1;
				mask1 |= (uint32_t)covered;

				CX1 -= FDY12;
				CX2 -= FDY23;
				CX3 -= FDY31;
			}

			CY1 += FDX12;
			CY2 += FDX23;
			CY3 += FDX31;
		}

		Mask0 = mask0;
		Mask1 = mask1;
	}
}

#else

void TriangleBlock::CoverageTest()
{
	// Corners of block
	int x0 = X << 4;
	int x1 = (X + q - 1) << 4;
	int y0 = Y << 4;
	int y1 = (Y + q - 1) << 4;

	__m128i mY = _mm_set_epi32(y0, y0, y1, y1);
	__m128i mX = _mm_set_epi32(x0, x0, x1, x1);

	// Evaluate half-space functions
	__m128i mCY1 = _mm_sub_epi32(
		_mm_add_epi32(mC1, _mm_shuffle_epi32(_mm_mul_epu32(mDX12, mY), _MM_SHUFFLE(0, 0, 2, 2))),
		_mm_shuffle_epi32(_mm_mul_epu32(mDY12, mX), _MM_SHUFFLE(0, 2, 0, 2)));
	__m128i mA = _mm_cmpgt_epi32(mCY1, _mm_setzero_si128());

	__m128i mCY2 = _mm_sub_epi32(
		_mm_add_epi32(mC2, _mm_shuffle_epi32(_mm_mul_epu32(mDX23, mY), _MM_SHUFFLE(0, 0, 2, 2))),
		_mm_shuffle_epi32(_mm_mul_epu32(mDY23, mX), _MM_SHUFFLE(0, 2, 0, 2)));
	__m128i mB = _mm_cmpgt_epi32(mCY2, _mm_setzero_si128());

	__m128i mCY3 = _mm_sub_epi32(
		_mm_add_epi32(mC3, _mm_shuffle_epi32(_mm_mul_epu32(mDX31, mY), _MM_SHUFFLE(0, 0, 2, 2))),
		_mm_shuffle_epi32(_mm_mul_epu32(mDY31, mX), _MM_SHUFFLE(0, 2, 0, 2)));
	__m128i mC = _mm_cmpgt_epi32(mCY3, _mm_setzero_si128());

	int abc = _mm_movemask_epi8(_mm_packs_epi16(_mm_packs_epi32(mA, mB), _mm_packs_epi32(mC, _mm_setzero_si128())));

	if ((abc & 0xf) == 0 || (abc & 0xf0) == 0 || (abc & 0xf00) == 0) // Skip block when outside an edge
	{
		Mask0 = 0;
		Mask1 = 0;
	}
	else if (abc == 0xfff) // Accept whole block when totally covered
	{
		Mask0 = 0xffffffff;
		Mask1 = 0xffffffff;
	}
	else // Partially covered block
	{
		uint32_t mask0 = 0;
		uint32_t mask1 = 0;

		mCY1 = _mm_sub_epi32(_mm_shuffle_epi32(mCY1, _MM_SHUFFLE(0, 0, 0, 0)), mFDY12Offset);
		mCY2 = _mm_sub_epi32(_mm_shuffle_epi32(mCY2, _MM_SHUFFLE(0, 0, 0, 0)), mFDY23Offset);
		mCY3 = _mm_sub_epi32(_mm_shuffle_epi32(mCY3, _MM_SHUFFLE(0, 0, 0, 0)), mFDY31Offset);
		for (int iy = 0; iy < 2; iy++)
		{
			__m128i mtest0 = _mm_cmpgt_epi32(mCY1, _mm_setzero_si128());
			mtest0 = _mm_and_si128(_mm_cmpgt_epi32(mCY2, _mm_setzero_si128()), mtest0);
			mtest0 = _mm_and_si128(_mm_cmpgt_epi32(mCY3, _mm_setzero_si128()), mtest0);
			__m128i mtest1 = _mm_cmpgt_epi32(_mm_sub_epi32(mCY1, mFDY12x4), _mm_setzero_si128());
			mtest1 = _mm_and_si128(_mm_cmpgt_epi32(_mm_sub_epi32(mCY2, mFDY23x4), _mm_setzero_si128()), mtest1);
			mtest1 = _mm_and_si128(_mm_cmpgt_epi32(_mm_sub_epi32(mCY3, mFDY31x4), _mm_setzero_si128()), mtest1);
			mCY1 = _mm_add_epi32(mCY1, mFDX12);
			mCY2 = _mm_add_epi32(mCY2, mFDX23);
			mCY3 = _mm_add_epi32(mCY3, mFDX31);
			__m128i first = _mm_packs_epi32(_mm_shuffle_epi32(mtest1, _MM_SHUFFLE(0, 1, 2, 3)), _mm_shuffle_epi32(mtest0, _MM_SHUFFLE(0, 1, 2, 3)));

			mtest0 = _mm_cmpgt_epi32(mCY1, _mm_setzero_si128());
			mtest0 = _mm_and_si128(_mm_cmpgt_epi32(mCY2, _mm_setzero_si128()), mtest0);
			mtest0 = _mm_and_si128(_mm_cmpgt_epi32(mCY3, _mm_setzero_si128()), mtest0);
			mtest1 = _mm_cmpgt_epi32(_mm_sub_epi32(mCY1, mFDY12x4), _mm_setzero_si128());
			mtest1 = _mm_and_si128(_mm_cmpgt_epi32(_mm_sub_epi32(mCY2, mFDY23x4), _mm_setzero_si128()), mtest1);
			mtest1 = _mm_and_si128(_mm_cmpgt_epi32(_mm_sub_epi32(mCY3, mFDY31x4), _mm_setzero_si128()), mtest1);
			mCY1 = _mm_add_epi32(mCY1, mFDX12);
			mCY2 = _mm_add_epi32(mCY2, mFDX23);
			mCY3 = _mm_add_epi32(mCY3, mFDX31);
			__m128i second = _mm_packs_epi32(_mm_shuffle_epi32(mtest1, _MM_SHUFFLE(0, 1, 2, 3)), _mm_shuffle_epi32(mtest0, _MM_SHUFFLE(0, 1, 2, 3)));

			mask0 <<= 16;
			mask0 |= _mm_movemask_epi8(_mm_packs_epi16(second, first));
		}

		for (int iy = 0; iy < 2; iy++)
		{
			__m128i mtest0 = _mm_cmpgt_epi32(mCY1, _mm_setzero_si128());
			mtest0 = _mm_and_si128(_mm_cmpgt_epi32(mCY2, _mm_setzero_si128()), mtest0);
			mtest0 = _mm_and_si128(_mm_cmpgt_epi32(mCY3, _mm_setzero_si128()), mtest0);
			__m128i mtest1 = _mm_cmpgt_epi32(_mm_sub_epi32(mCY1, mFDY12x4), _mm_setzero_si128());
			mtest1 = _mm_and_si128(_mm_cmpgt_epi32(_mm_sub_epi32(mCY2, mFDY23x4), _mm_setzero_si128()), mtest1);
			mtest1 = _mm_and_si128(_mm_cmpgt_epi32(_mm_sub_epi32(mCY3, mFDY31x4), _mm_setzero_si128()), mtest1);
			mCY1 = _mm_add_epi32(mCY1, mFDX12);
			mCY2 = _mm_add_epi32(mCY2, mFDX23);
			mCY3 = _mm_add_epi32(mCY3, mFDX31);
			__m128i first = _mm_packs_epi32(_mm_shuffle_epi32(mtest1, _MM_SHUFFLE(0, 1, 2, 3)), _mm_shuffle_epi32(mtest0, _MM_SHUFFLE(0, 1, 2, 3)));

			mtest0 = _mm_cmpgt_epi32(mCY1, _mm_setzero_si128());
			mtest0 = _mm_and_si128(_mm_cmpgt_epi32(mCY2, _mm_setzero_si128()), mtest0);
			mtest0 = _mm_and_si128(_mm_cmpgt_epi32(mCY3, _mm_setzero_si128()), mtest0);
			mtest1 = _mm_cmpgt_epi32(_mm_sub_epi32(mCY1, mFDY12x4), _mm_setzero_si128());
			mtest1 = _mm_and_si128(_mm_cmpgt_epi32(_mm_sub_epi32(mCY2, mFDY23x4), _mm_setzero_si128()), mtest1);
			mtest1 = _mm_and_si128(_mm_cmpgt_epi32(_mm_sub_epi32(mCY3, mFDY31x4), _mm_setzero_si128()), mtest1);
			mCY1 = _mm_add_epi32(mCY1, mFDX12);
			mCY2 = _mm_add_epi32(mCY2, mFDX23);
			mCY3 = _mm_add_epi32(mCY3, mFDX31);
			__m128i second = _mm_packs_epi32(_mm_shuffle_epi32(mtest1, _MM_SHUFFLE(0, 1, 2, 3)), _mm_shuffle_epi32(mtest0, _MM_SHUFFLE(0, 1, 2, 3)));

			mask1 <<= 16;
			mask1 |= _mm_movemask_epi8(_mm_packs_epi16(second, first));
		}

		Mask0 = mask0;
		Mask1 = mask1;
	}
}

#endif

void TriangleBlock::StencilWrite()
{
	int block = (X >> 3) + (Y >> 3) * stencilPitch;
	uint8_t *stencilBlock = &stencilValues[block * 64];
	uint32_t &stencilBlockMask = stencilMasks[block];
	uint32_t writeValue = stencilWriteValue;

	if (Mask0 == 0xffffffff && Mask1 == 0xffffffff)
	{
		stencilBlockMask = 0xffffff00 | writeValue;
	}
	else
	{
		uint32_t mask0 = Mask0;
		uint32_t mask1 = Mask1;

		bool isSingleValue = (stencilBlockMask & 0xffffff00) == 0xffffff00;
		if (isSingleValue)
		{
			uint8_t value = stencilBlockMask & 0xff;
			for (int v = 0; v < 64; v++)
				stencilBlock[v] = value;
			stencilBlockMask = 0;
		}

		int count = 0;
		for (int v = 0; v < 32; v++)
		{
			if ((mask0 & (1 << 31)) || stencilBlock[v] == writeValue)
			{
				stencilBlock[v] = writeValue;
				count++;
			}
			mask0 <<= 1;
		}
		for (int v = 32; v < 64; v++)
		{
			if ((mask1 & (1 << 31)) || stencilBlock[v] == writeValue)
			{
				stencilBlock[v] = writeValue;
				count++;
			}
			mask1 <<= 1;
		}

		if (count == 64)
			stencilBlockMask = 0xffffff00 | writeValue;
	}
}

#ifdef NO_SSE

void TriangleBlock::DepthWrite(const TriDrawTriangleArgs *args)
{
	int block = (X >> 3) + (Y >> 3) * zbufferPitch;
	float *depth = zbuffer + block * 64;

	const ShadedTriVertex &v1 = *args->v1;

	float stepXW = args->gradientX.W;
	float stepYW = args->gradientY.W;
	float posYW = v1.w + stepXW * (X - v1.x) + stepYW * (Y - v1.y) + args->depthOffset;

	if (Mask0 == 0xffffffff && Mask1 == 0xffffffff)
	{
		for (int iy = 0; iy < 8; iy++)
		{
			float posXW = posYW;
			for (int ix = 0; ix < 8; ix++)
			{
				*(depth++) = posXW;
				posXW += stepXW;
			}
			posYW += stepYW;
		}
	}
	else
	{
		uint32_t mask0 = Mask0;
		uint32_t mask1 = Mask1;

		for (int iy = 0; iy < 4; iy++)
		{
			float posXW = posYW;
			for (int ix = 0; ix < 8; ix++)
			{
				if (mask0 & (1 << 31))
					*depth = posXW;
				posXW += stepXW;
				mask0 <<= 1;
				depth++;
			}
			posYW += stepYW;
		}

		for (int iy = 0; iy < 4; iy++)
		{
			float posXW = posYW;
			for (int ix = 0; ix < 8; ix++)
			{
				if (mask1 & (1 << 31))
					*depth = posXW;
				posXW += stepXW;
				mask1 <<= 1;
				depth++;
			}
			posYW += stepYW;
		}
	}
}

#else

void TriangleBlock::DepthWrite(const TriDrawTriangleArgs *args)
{
	int block = (X >> 3) + (Y >> 3) * zbufferPitch;
	float *depth = zbuffer + block * 64;

	const ShadedTriVertex &v1 = *args->v1;

	float stepXW = args->gradientX.W;
	float stepYW = args->gradientY.W;
	float posYW = v1.w + stepXW * (X - v1.x) + stepYW * (Y - v1.y) + args->depthOffset;

	__m128 mposYW = _mm_setr_ps(posYW, posYW + stepXW, posYW + stepXW + stepXW, posYW + stepXW + stepXW + stepXW);
	__m128 mstepXW = _mm_set1_ps(stepXW * 4.0f);
	__m128 mstepYW = _mm_set1_ps(stepYW);

	if (Mask0 == 0xffffffff && Mask1 == 0xffffffff)
	{
		for (int iy = 0; iy < 8; iy++)
		{
			__m128 mposXW = mposYW;
			_mm_storeu_ps(depth, mposXW); depth += 4; mposXW = _mm_add_ps(mposXW, mstepXW);
			_mm_storeu_ps(depth, mposXW); depth += 4;
			mposYW = _mm_add_ps(mposYW, mstepYW);
		}
	}
	else
	{
		__m128i mxormask = _mm_set1_epi32(0xffffffff);
		__m128i topfour = _mm_setr_epi32(1 << 31, 1 << 30, 1 << 29, 1 << 28);

		__m128i mmask0 = _mm_set1_epi32(Mask0);
		__m128i mmask1 = _mm_set1_epi32(Mask1);

		for (int iy = 0; iy < 4; iy++)
		{
			__m128 mposXW = mposYW;
			_mm_maskmoveu_si128(_mm_castps_si128(mposXW), _mm_xor_si128(_mm_cmpeq_epi32(_mm_and_si128(mmask0, topfour), _mm_setzero_si128()), mxormask), (char*)depth); mmask0 = _mm_slli_epi32(mmask0, 4); depth += 4; mposXW = _mm_add_ps(mposXW, mstepXW);
			_mm_maskmoveu_si128(_mm_castps_si128(mposXW), _mm_xor_si128(_mm_cmpeq_epi32(_mm_and_si128(mmask0, topfour), _mm_setzero_si128()), mxormask), (char*)depth); mmask0 = _mm_slli_epi32(mmask0, 4); depth += 4;
			mposYW = _mm_add_ps(mposYW, mstepYW);
		}

		for (int iy = 0; iy < 4; iy++)
		{
			__m128 mposXW = mposYW;
			_mm_maskmoveu_si128(_mm_castps_si128(mposXW), _mm_xor_si128(_mm_cmpeq_epi32(_mm_and_si128(mmask1, topfour), _mm_setzero_si128()), mxormask), (char*)depth); mmask1 = _mm_slli_epi32(mmask1, 4); depth += 4; mposXW = _mm_add_ps(mposXW, mstepXW);
			_mm_maskmoveu_si128(_mm_castps_si128(mposXW), _mm_xor_si128(_mm_cmpeq_epi32(_mm_and_si128(mmask1, topfour), _mm_setzero_si128()), mxormask), (char*)depth); mmask1 = _mm_slli_epi32(mmask1, 4); depth += 4;
			mposYW = _mm_add_ps(mposYW, mstepYW);
		}
	}
}

#endif

void ScreenTriangle::Draw(const TriDrawTriangleArgs *args, PolyTriangleThreadData *thread)
{
	TriangleBlock block(args, thread);
	block.Render();
}

static void SortVertices(const TriDrawTriangleArgs *args, ShadedTriVertex **sortedVertices)
{
	sortedVertices[0] = args->v1;
	sortedVertices[1] = args->v2;
	sortedVertices[2] = args->v3;

	if (sortedVertices[1]->y < sortedVertices[0]->y)
		std::swap(sortedVertices[0], sortedVertices[1]);
	if (sortedVertices[2]->y < sortedVertices[0]->y)
		std::swap(sortedVertices[0], sortedVertices[2]);
	if (sortedVertices[2]->y < sortedVertices[1]->y)
		std::swap(sortedVertices[1], sortedVertices[2]);
}

void ScreenTriangle::DrawSWRender(const TriDrawTriangleArgs *args, PolyTriangleThreadData *thread)
{
	// Sort vertices by Y position
	ShadedTriVertex *sortedVertices[3];
	SortVertices(args, sortedVertices);

	int clipright = args->clipright;
	int clipbottom = args->clipbottom;

	// Ranges that different triangles edges are active
	int topY = (int)(sortedVertices[0]->y + 0.5f);
	int midY = (int)(sortedVertices[1]->y + 0.5f);
	int bottomY = (int)(sortedVertices[2]->y + 0.5f);

	topY = MAX(topY, 0);
	midY = clamp(midY, 0, clipbottom);
	bottomY = MIN(bottomY, clipbottom);

	if (topY >= bottomY)
		return;

	// Find start/end X positions for each line covered by the triangle:

	int leftEdge[MAXHEIGHT];
	int rightEdge[MAXHEIGHT];

	float longDX = sortedVertices[2]->x - sortedVertices[0]->x;
	float longDY = sortedVertices[2]->y - sortedVertices[0]->y;
	float longStep = longDX / longDY;
	float longPos = sortedVertices[0]->x + longStep * (topY + 0.5f - sortedVertices[0]->y) + 0.5f;

	if (topY < midY)
	{
		float shortDX = sortedVertices[1]->x - sortedVertices[0]->x;
		float shortDY = sortedVertices[1]->y - sortedVertices[0]->y;
		float shortStep = shortDX / shortDY;
		float shortPos = sortedVertices[0]->x + shortStep * (topY + 0.5f - sortedVertices[0]->y) + 0.5f;

		for (int y = topY; y < midY; y++)
		{
			int x0 = (int)shortPos;
			int x1 = (int)longPos;
			if (x1 < x0) std::swap(x0, x1);
			x0 = clamp(x0, 0, clipright);
			x1 = clamp(x1, 0, clipright);

			leftEdge[y] = x0;
			rightEdge[y] = x1;

			shortPos += shortStep;
			longPos += longStep;
		}
	}

	if (midY < bottomY)
	{
		float shortDX = sortedVertices[2]->x - sortedVertices[1]->x;
		float shortDY = sortedVertices[2]->y - sortedVertices[1]->y;
		float shortStep = shortDX / shortDY;
		float shortPos = sortedVertices[1]->x + shortStep * (midY + 0.5f - sortedVertices[1]->y) + 0.5f;

		for (int y = midY; y < bottomY; y++)
		{
			int x0 = (int)shortPos;
			int x1 = (int)longPos;
			if (x1 < x0) std::swap(x0, x1);
			x0 = clamp(x0, 0, clipright);
			x1 = clamp(x1, 0, clipright);

			leftEdge[y] = x0;
			rightEdge[y] = x1;

			shortPos += shortStep;
			longPos += longStep;
		}
	}

	// Draw the triangle:

	int bmode = (int)args->uniforms->BlendMode();
	auto drawfunc = args->destBgra ? ScreenTriangle::SpanDrawers32[bmode] : ScreenTriangle::SpanDrawers8[bmode];

	float stepXW = args->gradientX.W;
	float v1X = args->v1->x;
	float v1Y = args->v1->y;
	float v1W = args->v1->w;

	int num_cores = thread->num_cores;
	for (int y = topY + thread->skipped_by_thread(topY); y < bottomY; y += num_cores)
	{
		int x = leftEdge[y];
		int xend = rightEdge[y];

		float *zbufferLine = args->zbuffer + args->stencilPitch * 8 * y;

		float startX = x + (0.5f - v1X);
		float startY = y + (0.5f - v1Y);
		float posXW = v1W + stepXW * startX + args->gradientY.W * startY + args->depthOffset;

#ifndef NO_SSE
		__m128 mstepXW = _mm_set1_ps(stepXW * 4.0f);
		__m128 mfirstStepXW = _mm_setr_ps(0.0f, stepXW, stepXW + stepXW, stepXW + stepXW + stepXW);
		while (x < xend)
		{
			int xstart = x;

			int xendsse = x + ((xend - x) & ~3);
			__m128 mposXW = _mm_add_ps(_mm_set1_ps(posXW), mfirstStepXW);
			while (_mm_movemask_ps(_mm_cmple_ps(_mm_loadu_ps(zbufferLine + x), mposXW)) == 15 && x < xendsse)
			{
				_mm_storeu_ps(zbufferLine + x, mposXW);
				mposXW = _mm_add_ps(mposXW, mstepXW);
				x += 4;
			}
			posXW = _mm_cvtss_f32(mposXW);

			while (zbufferLine[x] <= posXW && x < xend)
			{
				zbufferLine[x] = posXW;
				posXW += stepXW;
				x++;
			}

			if (x > xstart)
				drawfunc(y, xstart, x, args);

			xendsse = x + ((xend - x) & ~3);
			mposXW = _mm_add_ps(_mm_set1_ps(posXW), mfirstStepXW);
			while (_mm_movemask_ps(_mm_cmple_ps(_mm_loadu_ps(zbufferLine + x), mposXW)) == 0 && x < xendsse)
			{
				mposXW = _mm_add_ps(mposXW, mstepXW);
				x += 4;
			}
			posXW = _mm_cvtss_f32(mposXW);

			while (zbufferLine[x] > posXW && x < xend)
			{
				posXW += stepXW;
				x++;
			}
		}
#else
		while (x < xend)
		{
			int xstart = x;
			while (zbufferLine[x] <= posXW && x < xend)
			{
				zbufferLine[x] = posXW;
				posXW += stepXW;
				x++;
			}

			if (x > xstart)
				drawfunc(y, xstart, x, args);

			while (zbufferLine[x] > posXW && x < xend)
			{
				posXW += stepXW;
				x++;
			}
		}
#endif
	}
}

template<typename ModeT, typename OptT>
void DrawSpanOpt32(int y, int x0, int x1, const TriDrawTriangleArgs *args)
{
	using namespace TriScreenDrawerModes;

	float v1X, v1Y, v1W, v1U, v1V, v1WorldX, v1WorldY, v1WorldZ;
	float startX, startY;
	float stepW, stepU, stepV, stepWorldX, stepWorldY, stepWorldZ;
	float posW, posU, posV, posWorldX, posWorldY, posWorldZ;

	PolyLight *lights;
	int num_lights;
	float worldnormalX, worldnormalY, worldnormalZ;
	uint32_t dynlightcolor;
	const uint32_t *texPixels, *translation;
	int texWidth, texHeight;
	uint32_t fillcolor;
	int alpha;
	uint32_t light;
	fixed_t shade, lightpos, lightstep;
	uint32_t shade_fade_r, shade_fade_g, shade_fade_b, shade_light_r, shade_light_g, shade_light_b, desaturate, inv_desaturate;
	int16_t dynlights_r[MAXWIDTH / 16], dynlights_g[MAXWIDTH / 16], dynlights_b[MAXWIDTH / 16];
	int16_t posdynlight_r, posdynlight_g, posdynlight_b;
	fixed_t lightarray[MAXWIDTH / 16];

	v1X = args->v1->x;
	v1Y = args->v1->y;
	v1W = args->v1->w;
	v1U = args->v1->u * v1W;
	v1V = args->v1->v * v1W;
	startX = x0 + (0.5f - v1X);
	startY = y + (0.5f - v1Y);
	stepW = args->gradientX.W;
	stepU = args->gradientX.U;
	stepV = args->gradientX.V;
	posW = v1W + stepW * startX + args->gradientY.W * startY;
	posU = v1U + stepU * startX + args->gradientY.U * startY;
	posV = v1V + stepV * startX + args->gradientY.V * startY;

	texPixels = (const uint32_t*)args->uniforms->TexturePixels();
	translation = (const uint32_t*)args->uniforms->Translation();
	texWidth = args->uniforms->TextureWidth();
	texHeight = args->uniforms->TextureHeight();
	fillcolor = args->uniforms->Color();
	alpha = args->uniforms->Alpha();
	light = args->uniforms->Light();

	if (OptT::Flags & SWOPT_FixedLight)
	{
		light += light >> 7; // 255 -> 256
	}
	else
	{
		float globVis = args->uniforms->GlobVis() * (1.0f / 32.0f);

		shade = (fixed_t)((2.0f - (light + 12.0f) / 128.0f) * (float)FRACUNIT);
		lightpos = (fixed_t)(globVis * posW * (float)FRACUNIT);
		lightstep = (fixed_t)(globVis * stepW * (float)FRACUNIT);

		int affineOffset = x0 / 16 * 16 - x0;
		lightpos = lightpos + lightstep * affineOffset;
		lightstep = lightstep * 16;

		fixed_t maxvis = 24 * FRACUNIT / 32;
		fixed_t maxlight = 31 * FRACUNIT / 32;

		for (int x = x0 / 16; x <= x1 / 16 + 1; x++)
		{
			lightarray[x] = (FRACUNIT - clamp<fixed_t>(shade - MIN(maxvis, lightpos), 0, maxlight)) >> 8;
			lightpos += lightstep;
		}

		int offset = x0 >> 4;
		int t1 = x0 & 15;
		int t0 = 16 - t1;
		lightpos = (lightarray[offset] * t0 + lightarray[offset + 1] * t1);

		for (int x = x0 / 16; x <= x1 / 16; x++)
		{
			lightarray[x] = lightarray[x + 1] - lightarray[x];
		}
	}

	if (OptT::Flags & SWOPT_DynLights)
	{
		v1WorldX = args->v1->worldX * v1W;
		v1WorldY = args->v1->worldY * v1W;
		v1WorldZ = args->v1->worldZ * v1W;
		stepWorldX = args->gradientX.WorldX;
		stepWorldY = args->gradientX.WorldY;
		stepWorldZ = args->gradientX.WorldZ;
		posWorldX = v1WorldX + stepWorldX * startX + args->gradientY.WorldX * startY;
		posWorldY = v1WorldY + stepWorldY * startX + args->gradientY.WorldY * startY;
		posWorldZ = v1WorldZ + stepWorldZ * startX + args->gradientY.WorldZ * startY;

		lights = args->uniforms->Lights();
		num_lights = args->uniforms->NumLights();
		worldnormalX = args->uniforms->Normal().X;
		worldnormalY = args->uniforms->Normal().Y;
		worldnormalZ = args->uniforms->Normal().Z;
		dynlightcolor = args->uniforms->DynLightColor();

		// The normal vector cannot be uniform when drawing models. Calculate and use the face normal:
		if (worldnormalX == 0.0f && worldnormalY == 0.0f && worldnormalZ == 0.0f)
		{
			float dx1 = args->v2->worldX - args->v1->worldX;
			float dy1 = args->v2->worldY - args->v1->worldY;
			float dz1 = args->v2->worldZ - args->v1->worldZ;
			float dx2 = args->v3->worldX - args->v1->worldX;
			float dy2 = args->v3->worldY - args->v1->worldY;
			float dz2 = args->v3->worldZ - args->v1->worldZ;
			worldnormalX = dy1 * dz2 - dz1 * dy2;
			worldnormalY = dz1 * dx2 - dx1 * dz2;
			worldnormalZ = dx1 * dy2 - dy1 * dx2;
			float lensqr = worldnormalX * worldnormalX + worldnormalY * worldnormalY + worldnormalZ * worldnormalZ;
#ifndef NO_SSE
			float rcplen = _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(lensqr)));
#else
			float rcplen = 1.0f / sqrt(lensqr);
#endif
			worldnormalX *= rcplen;
			worldnormalY *= rcplen;
			worldnormalZ *= rcplen;
		}

		int affineOffset = x0 / 16 * 16 - x0;
		float posLightW = posW + stepW * affineOffset;
		posWorldX = posWorldX + stepWorldX * affineOffset;
		posWorldY = posWorldY + stepWorldY * affineOffset;
		posWorldZ = posWorldZ + stepWorldZ * affineOffset;
		float stepLightW = stepW * 16.0f;
		stepWorldX *= 16.0f;
		stepWorldY *= 16.0f;
		stepWorldZ *= 16.0f;

		for (int x = x0 / 16; x <= x1 / 16 + 1; x++)
		{
			uint32_t lit_r = RPART(dynlightcolor);
			uint32_t lit_g = GPART(dynlightcolor);
			uint32_t lit_b = BPART(dynlightcolor);

			float rcp_posW = 1.0f / posLightW;
			float worldposX = posWorldX * rcp_posW;
			float worldposY = posWorldY * rcp_posW;
			float worldposZ = posWorldZ * rcp_posW;
			for (int i = 0; i < num_lights; i++)
			{
				float lightposX = lights[i].x;
				float lightposY = lights[i].y;
				float lightposZ = lights[i].z;
				float light_radius = lights[i].radius;
				uint32_t light_color = lights[i].color;

				bool is_attenuated = light_radius < 0.0f;
				if (is_attenuated)
					light_radius = -light_radius;

				// L = light-pos
				// dist = sqrt(dot(L, L))
				// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
				float Lx = lightposX - worldposX;
				float Ly = lightposY - worldposY;
				float Lz = lightposZ - worldposZ;
				float dist2 = Lx * Lx + Ly * Ly + Lz * Lz;
#ifdef NO_SSE
				//float rcp_dist = 1.0f / sqrt(dist2);
				float rcp_dist = 1.0f / (dist2 * 0.01f);
#else
				float rcp_dist = _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(dist2)));
#endif
				float dist = dist2 * rcp_dist;
				float distance_attenuation = 256.0f - MIN(dist * light_radius, 256.0f);

				// The simple light type
				float simple_attenuation = distance_attenuation;

				// The point light type
				// diffuse = max(dot(N,normalize(L)),0) * attenuation
				Lx *= rcp_dist;
				Ly *= rcp_dist;
				Lz *= rcp_dist;
				float dotNL = worldnormalX * Lx + worldnormalY * Ly + worldnormalZ * Lz;
				float point_attenuation = MAX(dotNL, 0.0f) * distance_attenuation;

				uint32_t attenuation = (uint32_t)(is_attenuated ? (int32_t)point_attenuation : (int32_t)simple_attenuation);

				lit_r += (RPART(light_color) * attenuation) >> 8;
				lit_g += (GPART(light_color) * attenuation) >> 8;
				lit_b += (BPART(light_color) * attenuation) >> 8;
			}

			lit_r = MIN<uint32_t>(lit_r, 255);
			lit_g = MIN<uint32_t>(lit_g, 255);
			lit_b = MIN<uint32_t>(lit_b, 255);
			dynlights_r[x] = lit_r;
			dynlights_g[x] = lit_g;
			dynlights_b[x] = lit_b;

			posLightW += stepLightW;
			posWorldX += stepWorldX;
			posWorldY += stepWorldY;
			posWorldZ += stepWorldZ;
		}

		int offset = x0 >> 4;
		int t1 = x0 & 15;
		int t0 = 16 - t1;
		posdynlight_r = (dynlights_r[offset] * t0 + dynlights_r[offset + 1] * t1);
		posdynlight_g = (dynlights_g[offset] * t0 + dynlights_g[offset + 1] * t1);
		posdynlight_b = (dynlights_b[offset] * t0 + dynlights_b[offset + 1] * t1);

		for (int x = x0 / 16; x <= x1 / 16; x++)
		{
			dynlights_r[x] = dynlights_r[x + 1] - dynlights_r[x];
			dynlights_g[x] = dynlights_g[x + 1] - dynlights_g[x];
			dynlights_b[x] = dynlights_b[x + 1] - dynlights_b[x];
		}
	}

	if (OptT::Flags & SWOPT_ColoredFog)
	{
		shade_fade_r = args->uniforms->ShadeFadeRed();
		shade_fade_g = args->uniforms->ShadeFadeGreen();
		shade_fade_b = args->uniforms->ShadeFadeBlue();
		shade_light_r = args->uniforms->ShadeLightRed();
		shade_light_g = args->uniforms->ShadeLightGreen();
		shade_light_b = args->uniforms->ShadeLightBlue();
		desaturate = args->uniforms->ShadeDesaturate();
		inv_desaturate = 256 - desaturate;
	}

	fixed_t fuzzscale;
	int _fuzzpos;
	if (ModeT::BlendOp == STYLEOP_Fuzz)
	{
		fuzzscale = (200 << FRACBITS) / viewheight;
		_fuzzpos = swrenderer::fuzzpos;
	}

	uint32_t *dest = (uint32_t*)args->dest;
	uint32_t *destLine = dest + args->pitch * y;

	int x = x0;
	while (x < x1)
	{
		if (ModeT::BlendOp == STYLEOP_Fuzz)
		{
			using namespace swrenderer;

			float rcpW = 0x01000000 / posW;
			int32_t u = (int32_t)(posU * rcpW);
			int32_t v = (int32_t)(posV * rcpW);
			uint32_t texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
			uint32_t texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;
			unsigned int sampleshadeout = APART(texPixels[texelX * texHeight + texelY]);
			sampleshadeout += sampleshadeout >> 7; // 255 -> 256

			int scaled_x = (x * fuzzscale) >> FRACBITS;
			int fuzz_x = fuzz_random_x_offset[scaled_x % FUZZ_RANDOM_X_SIZE] + _fuzzpos;

			fixed_t fuzzcount = FUZZTABLE << FRACBITS;
			fixed_t fuzz = ((fuzz_x << FRACBITS) + y * fuzzscale) % fuzzcount;
			unsigned int alpha = fuzzoffset[fuzz >> FRACBITS];

			sampleshadeout = (sampleshadeout * alpha) >> 5;

			uint32_t a = 256 - sampleshadeout;

			uint32_t dest = destLine[x];
			uint32_t out_r = (RPART(dest) * a) >> 8;
			uint32_t out_g = (GPART(dest) * a) >> 8;
			uint32_t out_b = (BPART(dest) * a) >> 8;
			destLine[x] = MAKEARGB(255, out_r, out_g, out_b);
		}
		else if (ModeT::SWFlags & SWSTYLEF_Skycap)
		{
			float rcpW = 0x01000000 / posW;
			int32_t u = (int32_t)(posU * rcpW);
			int32_t v = (int32_t)(posV * rcpW);
			uint32_t texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
			uint32_t texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;

			uint32_t fg = texPixels[texelX * texHeight + texelY];

			int start_fade = 2; // How fast it should fade out
			int alpha_top = clamp(v >> (16 - start_fade), 0, 256);
			int alpha_bottom = clamp(((2 << 24) - v) >> (16 - start_fade), 0, 256);
			int a = MIN(alpha_top, alpha_bottom);
			int inv_a = 256 - a;

			if (a == 256)
			{
				destLine[x] = fg;
			}
			else
			{
				uint32_t r = RPART(fg);
				uint32_t g = GPART(fg);
				uint32_t b = BPART(fg);
				uint32_t fg_a = APART(fg);
				uint32_t bg_red = RPART(fillcolor);
				uint32_t bg_green = GPART(fillcolor);
				uint32_t bg_blue = BPART(fillcolor);
				r = (r * a + bg_red * inv_a + 127) >> 8;
				g = (g * a + bg_green * inv_a + 127) >> 8;
				b = (b * a + bg_blue * inv_a + 127) >> 8;

				destLine[x] = MAKEARGB(255, r, g, b);
			}
		}
		else if (ModeT::SWFlags & SWSTYLEF_FogBoundary)
		{
			uint32_t fg = destLine[x];

			int lightshade;
			if (OptT::Flags & SWOPT_FixedLight)
			{
				lightshade = light;
			}
			else
			{
				lightshade = lightpos >> 4;
			}

			uint32_t shadedfg_r, shadedfg_g, shadedfg_b;
			if (OptT::Flags & SWOPT_ColoredFog)
			{
				uint32_t fg_r = RPART(fg);
				uint32_t fg_g = GPART(fg);
				uint32_t fg_b = BPART(fg);
				uint32_t intensity = ((fg_r * 77 + fg_g * 143 + fg_b * 37) >> 8) * desaturate;
				int inv_light = 256 - lightshade;
				shadedfg_r = (((shade_fade_r * inv_light + ((fg_r * inv_desaturate + intensity) >> 8) * lightshade) >> 8) * shade_light_r) >> 8;
				shadedfg_g = (((shade_fade_g * inv_light + ((fg_g * inv_desaturate + intensity) >> 8) * lightshade) >> 8) * shade_light_g) >> 8;
				shadedfg_b = (((shade_fade_b * inv_light + ((fg_b * inv_desaturate + intensity) >> 8) * lightshade) >> 8) * shade_light_b) >> 8;
			}
			else
			{
				shadedfg_r = (RPART(fg) * lightshade) >> 8;
				shadedfg_g = (GPART(fg) * lightshade) >> 8;
				shadedfg_b = (BPART(fg) * lightshade) >> 8;
			}

			destLine[x] = MAKEARGB(255, shadedfg_r, shadedfg_g, shadedfg_b);
		}
		else
		{
			uint32_t fg = 0;

			if (ModeT::SWFlags & SWSTYLEF_Fill)
			{
				fg = fillcolor;
			}
			else
			{
				float rcpW = 0x01000000 / posW;
				int32_t u = (int32_t)(posU * rcpW);
				int32_t v = (int32_t)(posV * rcpW);
				uint32_t texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
				uint32_t texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;

				if (ModeT::SWFlags & SWSTYLEF_Translated)
				{
					fg = translation[((const uint8_t*)texPixels)[texelX * texHeight + texelY]];
				}
				else if (ModeT::Flags & STYLEF_RedIsAlpha)
				{
					fg = ((const uint8_t*)texPixels)[texelX * texHeight + texelY];
				}
				else
				{
					fg = texPixels[texelX * texHeight + texelY];
				}
			}

			if ((ModeT::Flags & STYLEF_ColorIsFixed) && !(ModeT::SWFlags & SWSTYLEF_Fill))
			{
				if (ModeT::Flags & STYLEF_RedIsAlpha)
					fg = (fg << 24) | (fillcolor & 0x00ffffff);
				else
					fg = (fg & 0xff000000) | (fillcolor & 0x00ffffff);
			}

			uint32_t fgalpha = fg >> 24;

			if (!(ModeT::Flags & STYLEF_Alpha1))
			{
				fgalpha = (fgalpha * alpha) >> 8;
			}

			int lightshade;
			if (OptT::Flags & SWOPT_FixedLight)
			{
				lightshade = light;
			}
			else
			{
				lightshade = lightpos >> 4;
			}

			uint32_t shadedfg_r, shadedfg_g, shadedfg_b;
			if (OptT::Flags & SWOPT_ColoredFog)
			{
				uint32_t fg_r = RPART(fg);
				uint32_t fg_g = GPART(fg);
				uint32_t fg_b = BPART(fg);
				uint32_t intensity = ((fg_r * 77 + fg_g * 143 + fg_b * 37) >> 8) * desaturate;
				int inv_light = 256 - lightshade;
				shadedfg_r = (((shade_fade_r * inv_light + ((fg_r * inv_desaturate + intensity) >> 8) * lightshade) >> 8) * shade_light_r) >> 8;
				shadedfg_g = (((shade_fade_g * inv_light + ((fg_g * inv_desaturate + intensity) >> 8) * lightshade) >> 8) * shade_light_g) >> 8;
				shadedfg_b = (((shade_fade_b * inv_light + ((fg_b * inv_desaturate + intensity) >> 8) * lightshade) >> 8) * shade_light_b) >> 8;

				if (OptT::Flags & SWOPT_DynLights)
				{
					uint32_t lit_r = posdynlight_r >> 4;
					uint32_t lit_g = posdynlight_g >> 4;
					uint32_t lit_b = posdynlight_b >> 4;
					shadedfg_r = MIN(shadedfg_r + ((fg_r * lit_r) >> 8), (uint32_t)255);
					shadedfg_g = MIN(shadedfg_g + ((fg_g * lit_g) >> 8), (uint32_t)255);
					shadedfg_b = MIN(shadedfg_b + ((fg_b * lit_b) >> 8), (uint32_t)255);
				}
			}
			else
			{
				if (OptT::Flags & SWOPT_DynLights)
				{
					uint32_t lit_r = posdynlight_r >> 4;
					uint32_t lit_g = posdynlight_g >> 4;
					uint32_t lit_b = posdynlight_b >> 4;
					shadedfg_r = (RPART(fg) * MIN(lightshade + lit_r, (uint32_t)256)) >> 8;
					shadedfg_g = (GPART(fg) * MIN(lightshade + lit_g, (uint32_t)256)) >> 8;
					shadedfg_b = (BPART(fg) * MIN(lightshade + lit_b, (uint32_t)256)) >> 8;
				}
				else
				{
					shadedfg_r = (RPART(fg) * lightshade) >> 8;
					shadedfg_g = (GPART(fg) * lightshade) >> 8;
					shadedfg_b = (BPART(fg) * lightshade) >> 8;
				}
			}

			if (ModeT::BlendSrc == STYLEALPHA_One && ModeT::BlendDest == STYLEALPHA_Zero)
			{
				destLine[x] = MAKEARGB(255, shadedfg_r, shadedfg_g, shadedfg_b);
			}
			else if (ModeT::BlendSrc == STYLEALPHA_One && ModeT::BlendDest == STYLEALPHA_One)
			{
				uint32_t dest = destLine[x];

				if (ModeT::BlendOp == STYLEOP_Add)
				{
					uint32_t out_r = MIN<uint32_t>(RPART(dest) + shadedfg_r, 255);
					uint32_t out_g = MIN<uint32_t>(GPART(dest) + shadedfg_g, 255);
					uint32_t out_b = MIN<uint32_t>(BPART(dest) + shadedfg_b, 255);
					destLine[x] = MAKEARGB(255, out_r, out_g, out_b);
				}
				else if (ModeT::BlendOp == STYLEOP_RevSub)
				{
					uint32_t out_r = MAX<uint32_t>(RPART(dest) - shadedfg_r, 0);
					uint32_t out_g = MAX<uint32_t>(GPART(dest) - shadedfg_g, 0);
					uint32_t out_b = MAX<uint32_t>(BPART(dest) - shadedfg_b, 0);
					destLine[x] = MAKEARGB(255, out_r, out_g, out_b);
				}
				else //if (ModeT::BlendOp == STYLEOP_Sub)
				{
					uint32_t out_r = MAX<uint32_t>(shadedfg_r - RPART(dest), 0);
					uint32_t out_g = MAX<uint32_t>(shadedfg_g - GPART(dest), 0);
					uint32_t out_b = MAX<uint32_t>(shadedfg_b - BPART(dest), 0);
					destLine[x] = MAKEARGB(255, out_r, out_g, out_b);
				}
			}
			else if (ModeT::SWFlags & SWSTYLEF_SrcColorOneMinusSrcColor)
			{
				uint32_t dest = destLine[x];

				uint32_t sfactor_r = shadedfg_r; sfactor_r += sfactor_r >> 7; // 255 -> 256
				uint32_t sfactor_g = shadedfg_g; sfactor_g += sfactor_g >> 7; // 255 -> 256
				uint32_t sfactor_b = shadedfg_b; sfactor_b += sfactor_b >> 7; // 255 -> 256
				uint32_t sfactor_a = fgalpha; sfactor_a += sfactor_a >> 7; // 255 -> 256
				uint32_t dfactor_r = 256 - sfactor_r;
				uint32_t dfactor_g = 256 - sfactor_g;
				uint32_t dfactor_b = 256 - sfactor_b;
				uint32_t out_r = (RPART(dest) * dfactor_r + shadedfg_r * sfactor_r + 128) >> 8;
				uint32_t out_g = (GPART(dest) * dfactor_g + shadedfg_g * sfactor_g + 128) >> 8;
				uint32_t out_b = (BPART(dest) * dfactor_b + shadedfg_b * sfactor_b + 128) >> 8;

				destLine[x] = MAKEARGB(255, out_r, out_g, out_b);
			}
			else if (ModeT::BlendSrc == STYLEALPHA_Src && ModeT::BlendDest == STYLEALPHA_InvSrc && fgalpha == 255)
			{
				destLine[x] = MAKEARGB(255, shadedfg_r, shadedfg_g, shadedfg_b);
			}
			else if (ModeT::BlendSrc != STYLEALPHA_Src || ModeT::BlendDest != STYLEALPHA_InvSrc || fgalpha != 0)
			{
				uint32_t dest = destLine[x];

				uint32_t sfactor = fgalpha; sfactor += sfactor >> 7; // 255 -> 256
				uint32_t src_r = shadedfg_r * sfactor;
				uint32_t src_g = shadedfg_g * sfactor;
				uint32_t src_b = shadedfg_b * sfactor;
				uint32_t dest_r = RPART(dest);
				uint32_t dest_g = GPART(dest);
				uint32_t dest_b = BPART(dest);
				if (ModeT::BlendDest == STYLEALPHA_One)
				{
					dest_r <<= 8;
					dest_g <<= 8;
					dest_b <<= 8;
				}
				else
				{
					uint32_t dfactor = 256 - sfactor;
					dest_r *= dfactor;
					dest_g *= dfactor;
					dest_b *= dfactor;
				}

				uint32_t out_r, out_g, out_b;
				if (ModeT::BlendOp == STYLEOP_Add)
				{
					if (ModeT::BlendDest == STYLEALPHA_One)
					{
						out_r = MIN<int32_t>((dest_r + src_r + 128) >> 8, 255);
						out_g = MIN<int32_t>((dest_g + src_g + 128) >> 8, 255);
						out_b = MIN<int32_t>((dest_b + src_b + 128) >> 8, 255);
					}
					else
					{
						out_r = (dest_r + src_r + 128) >> 8;
						out_g = (dest_g + src_g + 128) >> 8;
						out_b = (dest_b + src_b + 128) >> 8;
					}
				}
				else if (ModeT::BlendOp == STYLEOP_RevSub)
				{
					out_r = MAX<int32_t>(static_cast<int32_t>(dest_r - src_r + 128) >> 8, 0);
					out_g = MAX<int32_t>(static_cast<int32_t>(dest_g - src_g + 128) >> 8, 0);
					out_b = MAX<int32_t>(static_cast<int32_t>(dest_b - src_b + 128) >> 8, 0);
				}
				else //if (ModeT::BlendOp == STYLEOP_Sub)
				{
					out_r = MAX<int32_t>(static_cast<int32_t>(src_r - dest_r + 128) >> 8, 0);
					out_g = MAX<int32_t>(static_cast<int32_t>(src_g - dest_g + 128) >> 8, 0);
					out_b = MAX<int32_t>(static_cast<int32_t>(src_b - dest_b + 128) >> 8, 0);
				}

				destLine[x] = MAKEARGB(255, out_r, out_g, out_b);
			}
		}

		posW += stepW;
		posU += stepU;
		posV += stepV;
		if (OptT::Flags & SWOPT_DynLights)
		{
			posdynlight_r += dynlights_r[x >> 4];
			posdynlight_g += dynlights_g[x >> 4];
			posdynlight_b += dynlights_b[x >> 4];
		}
		if (!(OptT::Flags & SWOPT_FixedLight))
			lightpos += lightarray[x >> 4];
		x++;
	}
}

template<typename ModeT>
void DrawSpan32(int y, int x0, int x1, const TriDrawTriangleArgs *args)
{
	using namespace TriScreenDrawerModes;

	if (args->uniforms->NumLights() == 0 && args->uniforms->DynLightColor() == 0)
	{
		if (!args->uniforms->FixedLight())
		{
			if (args->uniforms->SimpleShade())
				DrawSpanOpt32<ModeT, DrawerOpt>(y, x0, x1, args);
			else
				DrawSpanOpt32<ModeT, DrawerOptC>(y, x0, x1, args);
		}
		else
		{
			if (args->uniforms->SimpleShade())
				DrawSpanOpt32<ModeT, DrawerOptF>(y, x0, x1, args);
			else
				DrawSpanOpt32<ModeT, DrawerOptCF>(y, x0, x1, args);
		}
	}
	else
	{
		if (!args->uniforms->FixedLight())
		{
			if (args->uniforms->SimpleShade())
				DrawSpanOpt32<ModeT, DrawerOptL>(y, x0, x1, args);
			else
				DrawSpanOpt32<ModeT, DrawerOptLC>(y, x0, x1, args);
		}
		else
		{
			if (args->uniforms->SimpleShade())
				DrawSpanOpt32<ModeT, DrawerOptLF>(y, x0, x1, args);
			else
				DrawSpanOpt32<ModeT, DrawerOptLCF>(y, x0, x1, args);
		}
	}
}

template<typename ModeT, typename OptT>
void DrawSpanOpt8(int y, int x0, int x1, const TriDrawTriangleArgs *args)
{
	using namespace TriScreenDrawerModes;

	float v1X, v1Y, v1W, v1U, v1V, v1WorldX, v1WorldY, v1WorldZ;
	float startX, startY;
	float stepW, stepU, stepV, stepWorldX, stepWorldY, stepWorldZ;
	float posW, posU, posV, posWorldX, posWorldY, posWorldZ;

	PolyLight *lights;
	int num_lights;
	float worldnormalX, worldnormalY, worldnormalZ;
	uint32_t dynlightcolor;
	const uint8_t *colormaps, *texPixels, *translation;
	int texWidth, texHeight;
	uint32_t fillcolor, capcolor;
	int alpha;
	uint32_t light;
	fixed_t shade, lightpos, lightstep;
	int16_t dynlights_r[MAXWIDTH / 16], dynlights_g[MAXWIDTH / 16], dynlights_b[MAXWIDTH / 16];
	int16_t posdynlight_r, posdynlight_g, posdynlight_b;
	fixed_t lightarray[MAXWIDTH / 16];

	v1X = args->v1->x;
	v1Y = args->v1->y;
	v1W = args->v1->w;
	v1U = args->v1->u * v1W;
	v1V = args->v1->v * v1W;
	startX = x0 + (0.5f - v1X);
	startY = y + (0.5f - v1Y);
	stepW = args->gradientX.W;
	stepU = args->gradientX.U;
	stepV = args->gradientX.V;
	posW = v1W + stepW * startX + args->gradientY.W * startY;
	posU = v1U + stepU * startX + args->gradientY.U * startY;
	posV = v1V + stepV * startX + args->gradientY.V * startY;

	texPixels = args->uniforms->TexturePixels();
	translation = args->uniforms->Translation();
	texWidth = args->uniforms->TextureWidth();
	texHeight = args->uniforms->TextureHeight();
	fillcolor = args->uniforms->Color();
	alpha = args->uniforms->Alpha();
	colormaps = args->uniforms->BaseColormap();
	light = args->uniforms->Light();

	if (ModeT::SWFlags & SWSTYLEF_Skycap)
		capcolor = GPalette.BaseColors[fillcolor].d;

	if (OptT::Flags & SWOPT_FixedLight)
	{
		light += light >> 7; // 255 -> 256
		light = ((256 - light) * NUMCOLORMAPS) & 0xffffff00;
	}
	else
	{
		float globVis = args->uniforms->GlobVis() * (1.0f / 32.0f);

		shade = (fixed_t)((2.0f - (light + 12.0f) / 128.0f) * (float)FRACUNIT);
		lightpos = (fixed_t)(globVis * posW * (float)FRACUNIT);
		lightstep = (fixed_t)(globVis * stepW * (float)FRACUNIT);

		int affineOffset = x0 / 16 * 16 - x0;
		lightpos = lightpos + lightstep * affineOffset;
		lightstep = lightstep * 16;

		fixed_t maxvis = 24 * FRACUNIT / 32;
		fixed_t maxlight = 31 * FRACUNIT / 32;

		for (int x = x0 / 16; x <= x1 / 16 + 1; x++)
		{
			lightarray[x] = (clamp<fixed_t>(shade - MIN(maxvis, lightpos), 0, maxlight) >> 8) << 5;
			lightpos += lightstep;
		}

		int offset = x0 >> 4;
		int t1 = x0 & 15;
		int t0 = 16 - t1;
		lightpos = (lightarray[offset] * t0 + lightarray[offset + 1] * t1);

		for (int x = x0 / 16; x <= x1 / 16; x++)
		{
			lightarray[x] = lightarray[x + 1] - lightarray[x];
		}
	}

	if (OptT::Flags & SWOPT_DynLights)
	{
		v1WorldX = args->v1->worldX * v1W;
		v1WorldY = args->v1->worldY * v1W;
		v1WorldZ = args->v1->worldZ * v1W;
		stepWorldX = args->gradientX.WorldX;
		stepWorldY = args->gradientX.WorldY;
		stepWorldZ = args->gradientX.WorldZ;
		posWorldX = v1WorldX + stepWorldX * startX + args->gradientY.WorldX * startY;
		posWorldY = v1WorldY + stepWorldY * startX + args->gradientY.WorldY * startY;
		posWorldZ = v1WorldZ + stepWorldZ * startX + args->gradientY.WorldZ * startY;

		lights = args->uniforms->Lights();
		num_lights = args->uniforms->NumLights();
		worldnormalX = args->uniforms->Normal().X;
		worldnormalY = args->uniforms->Normal().Y;
		worldnormalZ = args->uniforms->Normal().Z;
		dynlightcolor = args->uniforms->DynLightColor();

		// The normal vector cannot be uniform when drawing models. Calculate and use the face normal:
		if (worldnormalX == 0.0f && worldnormalY == 0.0f && worldnormalZ == 0.0f)
		{
			float dx1 = args->v2->worldX - args->v1->worldX;
			float dy1 = args->v2->worldY - args->v1->worldY;
			float dz1 = args->v2->worldZ - args->v1->worldZ;
			float dx2 = args->v3->worldX - args->v1->worldX;
			float dy2 = args->v3->worldY - args->v1->worldY;
			float dz2 = args->v3->worldZ - args->v1->worldZ;
			worldnormalX = dy1 * dz2 - dz1 * dy2;
			worldnormalY = dz1 * dx2 - dx1 * dz2;
			worldnormalZ = dx1 * dy2 - dy1 * dx2;
			float lensqr = worldnormalX * worldnormalX + worldnormalY * worldnormalY + worldnormalZ * worldnormalZ;
#ifndef NO_SSE
			float rcplen = _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(lensqr)));
#else
			float rcplen = 1.0f / sqrt(lensqr);
#endif
			worldnormalX *= rcplen;
			worldnormalY *= rcplen;
			worldnormalZ *= rcplen;
		}

		int affineOffset = x0 / 16 * 16 - x0;
		float posLightW = posW + stepW * affineOffset;
		posWorldX = posWorldX + stepWorldX * affineOffset;
		posWorldY = posWorldY + stepWorldY * affineOffset;
		posWorldZ = posWorldZ + stepWorldZ * affineOffset;
		float stepLightW = stepW * 16.0f;
		stepWorldX *= 16.0f;
		stepWorldY *= 16.0f;
		stepWorldZ *= 16.0f;

		for (int x = x0 / 16; x <= x1 / 16 + 1; x++)
		{
			uint32_t lit_r = RPART(dynlightcolor);
			uint32_t lit_g = GPART(dynlightcolor);
			uint32_t lit_b = BPART(dynlightcolor);

			float rcp_posW = 1.0f / posLightW;
			float worldposX = posWorldX * rcp_posW;
			float worldposY = posWorldY * rcp_posW;
			float worldposZ = posWorldZ * rcp_posW;
			for (int i = 0; i < num_lights; i++)
			{
				float lightposX = lights[i].x;
				float lightposY = lights[i].y;
				float lightposZ = lights[i].z;
				float light_radius = lights[i].radius;
				uint32_t light_color = lights[i].color;

				bool is_attenuated = light_radius < 0.0f;
				if (is_attenuated)
					light_radius = -light_radius;

				// L = light-pos
				// dist = sqrt(dot(L, L))
				// distance_attenuation = 1 - MIN(dist * (1/radius), 1)
				float Lx = lightposX - worldposX;
				float Ly = lightposY - worldposY;
				float Lz = lightposZ - worldposZ;
				float dist2 = Lx * Lx + Ly * Ly + Lz * Lz;
#ifdef NO_SSE
				//float rcp_dist = 1.0f / sqrt(dist2);
				float rcp_dist = 1.0f / (dist2 * 0.01f);
#else
				float rcp_dist = _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(dist2)));
#endif
				float dist = dist2 * rcp_dist;
				float distance_attenuation = 256.0f - MIN(dist * light_radius, 256.0f);

				// The simple light type
				float simple_attenuation = distance_attenuation;

				// The point light type
				// diffuse = max(dot(N,normalize(L)),0) * attenuation
				Lx *= rcp_dist;
				Ly *= rcp_dist;
				Lz *= rcp_dist;
				float dotNL = worldnormalX * Lx + worldnormalY * Ly + worldnormalZ * Lz;
				float point_attenuation = MAX(dotNL, 0.0f) * distance_attenuation;

				uint32_t attenuation = (uint32_t)(is_attenuated ? (int32_t)point_attenuation : (int32_t)simple_attenuation);

				lit_r += (RPART(light_color) * attenuation) >> 8;
				lit_g += (GPART(light_color) * attenuation) >> 8;
				lit_b += (BPART(light_color) * attenuation) >> 8;
			}

			lit_r = MIN<uint32_t>(lit_r, 255);
			lit_g = MIN<uint32_t>(lit_g, 255);
			lit_b = MIN<uint32_t>(lit_b, 255);
			dynlights_r[x] = lit_r;
			dynlights_g[x] = lit_g;
			dynlights_b[x] = lit_b;

			posLightW += stepLightW;
			posWorldX += stepWorldX;
			posWorldY += stepWorldY;
			posWorldZ += stepWorldZ;
		}

		int offset = x0 >> 4;
		int t1 = x0 & 15;
		int t0 = 16 - t1;
		posdynlight_r = (dynlights_r[offset] * t0 + dynlights_r[offset + 1] * t1);
		posdynlight_g = (dynlights_g[offset] * t0 + dynlights_g[offset + 1] * t1);
		posdynlight_b = (dynlights_b[offset] * t0 + dynlights_b[offset + 1] * t1);

		for (int x = x0 / 16; x <= x1 / 16; x++)
		{
			dynlights_r[x] = dynlights_r[x + 1] - dynlights_r[x];
			dynlights_g[x] = dynlights_g[x + 1] - dynlights_g[x];
			dynlights_b[x] = dynlights_b[x + 1] - dynlights_b[x];
		}
	}

	fixed_t fuzzscale;
	int _fuzzpos;
	if (ModeT::BlendOp == STYLEOP_Fuzz)
	{
		fuzzscale = (200 << FRACBITS) / viewheight;
		_fuzzpos = swrenderer::fuzzpos;
	}

	uint8_t *dest = (uint8_t*)args->dest;
	uint8_t *destLine = dest + args->pitch * y;

	int x = x0;
	while (x < x1)
	{
		if (ModeT::BlendOp == STYLEOP_Fuzz)
		{
			using namespace swrenderer;

			float rcpW = 0x01000000 / posW;
			int32_t u = (int32_t)(posU * rcpW);
			int32_t v = (int32_t)(posV * rcpW);
			uint32_t texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
			uint32_t texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;
			unsigned int sampleshadeout = (texPixels[texelX * texHeight + texelY] != 0) ? 256 : 0;

			int scaled_x = (x * fuzzscale) >> FRACBITS;
			int fuzz_x = fuzz_random_x_offset[scaled_x % FUZZ_RANDOM_X_SIZE] + _fuzzpos;

			fixed_t fuzzcount = FUZZTABLE << FRACBITS;
			fixed_t fuzz = ((fuzz_x << FRACBITS) + y * fuzzscale) % fuzzcount;
			unsigned int alpha = fuzzoffset[fuzz >> FRACBITS];

			sampleshadeout = (sampleshadeout * alpha) >> 5;

			uint32_t a = 256 - sampleshadeout;

			uint32_t dest = GPalette.BaseColors[destLine[x]].d;
			uint32_t r = (RPART(dest) * a) >> 8;
			uint32_t g = (GPART(dest) * a) >> 8;
			uint32_t b = (BPART(dest) * a) >> 8;
			destLine[x] = RGB256k.All[((r >> 2) << 12) | ((g >> 2) << 6) | (b >> 2)];
		}
		else if (ModeT::SWFlags & SWSTYLEF_Skycap)
		{
			float rcpW = 0x01000000 / posW;
			int32_t u = (int32_t)(posU * rcpW);
			int32_t v = (int32_t)(posV * rcpW);
			uint32_t texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
			uint32_t texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;
			int fg = texPixels[texelX * texHeight + texelY];

			int start_fade = 2; // How fast it should fade out
			int alpha_top = clamp(v >> (16 - start_fade), 0, 256);
			int alpha_bottom = clamp(((2 << 24) - v) >> (16 - start_fade), 0, 256);
			int a = MIN(alpha_top, alpha_bottom);
			int inv_a = 256 - a;

			if (a == 256)
			{
				destLine[x] = fg;
			}
			else
			{
				uint32_t texelrgb = GPalette.BaseColors[fg].d;

				uint32_t r = RPART(texelrgb);
				uint32_t g = GPART(texelrgb);
				uint32_t b = BPART(texelrgb);
				uint32_t fg_a = APART(texelrgb);
				uint32_t bg_red = RPART(capcolor);
				uint32_t bg_green = GPART(capcolor);
				uint32_t bg_blue = BPART(capcolor);
				r = (r * a + bg_red * inv_a + 127) >> 8;
				g = (g * a + bg_green * inv_a + 127) >> 8;
				b = (b * a + bg_blue * inv_a + 127) >> 8;

				destLine[x] = RGB256k.All[((r >> 2) << 12) | ((g >> 2) << 6) | (b >> 2)];
			}
		}
		else if (ModeT::SWFlags & SWSTYLEF_FogBoundary)
		{
			int fg = destLine[x];

			uint8_t shadedfg;
			if (OptT::Flags & SWOPT_FixedLight)
			{
				shadedfg = colormaps[light + fg];
			}
			else
			{
				int lightshade = (lightpos >> 4) & 0xffffff00;
				shadedfg = colormaps[lightshade + fg];
			}

			destLine[x] = shadedfg;
		}
		else
		{
			int fg;
			if (ModeT::SWFlags & SWSTYLEF_Fill)
			{
				fg = fillcolor;
			}
			else
			{
				float rcpW = 0x01000000 / posW;
				int32_t u = (int32_t)(posU * rcpW);
				int32_t v = (int32_t)(posV * rcpW);
				uint32_t texelX = ((((uint32_t)u << 8) >> 16) * texWidth) >> 16;
				uint32_t texelY = ((((uint32_t)v << 8) >> 16) * texHeight) >> 16;
				fg = texPixels[texelX * texHeight + texelY];
			}

			int fgalpha = 255;

			if (ModeT::BlendDest == STYLEALPHA_InvSrc)
			{
				if (fg == 0)
					fgalpha = 0;
			}

			if ((ModeT::Flags & STYLEF_ColorIsFixed) && !(ModeT::SWFlags & SWSTYLEF_Fill))
			{
				if (ModeT::Flags & STYLEF_RedIsAlpha)
					fgalpha = fg;
				fg = fillcolor;
			}

			if (!(ModeT::Flags & STYLEF_Alpha1))
			{
				fgalpha = (fgalpha * alpha) >> 8;
			}

			if (ModeT::SWFlags & SWSTYLEF_Translated)
				fg = translation[fg];

			uint8_t shadedfg;
			if (OptT::Flags & SWOPT_FixedLight)
			{
				shadedfg = colormaps[light + fg];
			}
			else
			{
				int lightshade = (lightpos >> 4) & 0xffffff00;
				shadedfg = colormaps[lightshade + fg];
			}

			if (OptT::Flags & SWOPT_DynLights)
			{
				if (posdynlight_r | posdynlight_g | posdynlight_b)
				{
					uint32_t lit_r = posdynlight_r >> 4;
					uint32_t lit_g = posdynlight_g >> 4;
					uint32_t lit_b = posdynlight_b >> 4;

					uint32_t fgrgb = GPalette.BaseColors[fg];
					uint32_t shadedfgrgb = GPalette.BaseColors[shadedfg];

					uint32_t out_r = MIN(((RPART(fgrgb) * lit_r) >> 8) + RPART(shadedfgrgb), (uint32_t)255);
					uint32_t out_g = MIN(((GPART(fgrgb) * lit_g) >> 8) + GPART(shadedfgrgb), (uint32_t)255);
					uint32_t out_b = MIN(((BPART(fgrgb) * lit_b) >> 8) + BPART(shadedfgrgb), (uint32_t)255);
					shadedfg = RGB256k.All[((out_r >> 2) << 12) | ((out_g >> 2) << 6) | (out_b >> 2)];
				}
			}

			if (ModeT::BlendSrc == STYLEALPHA_One && ModeT::BlendDest == STYLEALPHA_Zero)
			{
				destLine[x] = shadedfg;
			}
			else if (ModeT::BlendSrc == STYLEALPHA_One && ModeT::BlendDest == STYLEALPHA_One)
			{
				uint32_t src = GPalette.BaseColors[shadedfg];
				uint32_t dest = GPalette.BaseColors[destLine[x]];

				if (ModeT::BlendOp == STYLEOP_Add)
				{
					uint32_t out_r = MIN<uint32_t>(RPART(dest) + RPART(src), 255);
					uint32_t out_g = MIN<uint32_t>(GPART(dest) + GPART(src), 255);
					uint32_t out_b = MIN<uint32_t>(BPART(dest) + BPART(src), 255);
					destLine[x] = RGB256k.All[((out_r >> 2) << 12) | ((out_g >> 2) << 6) | (out_b >> 2)];
				}
				else if (ModeT::BlendOp == STYLEOP_RevSub)
				{
					uint32_t out_r = MAX<uint32_t>(RPART(dest) - RPART(src), 0);
					uint32_t out_g = MAX<uint32_t>(GPART(dest) - GPART(src), 0);
					uint32_t out_b = MAX<uint32_t>(BPART(dest) - BPART(src), 0);
					destLine[x] = RGB256k.All[((out_r >> 2) << 12) | ((out_g >> 2) << 6) | (out_b >> 2)];
				}
				else //if (ModeT::BlendOp == STYLEOP_Sub)
				{
					uint32_t out_r = MAX<uint32_t>(RPART(src) - RPART(dest), 0);
					uint32_t out_g = MAX<uint32_t>(GPART(src) - GPART(dest), 0);
					uint32_t out_b = MAX<uint32_t>(BPART(src) - BPART(dest), 0);
					destLine[x] = RGB256k.All[((out_r >> 2) << 12) | ((out_g >> 2) << 6) | (out_b >> 2)];
				}
			}
			else if (ModeT::SWFlags & SWSTYLEF_SrcColorOneMinusSrcColor)
			{
				uint32_t src = GPalette.BaseColors[shadedfg];
				uint32_t dest = GPalette.BaseColors[destLine[x]];

				uint32_t sfactor_r = RPART(src); sfactor_r += sfactor_r >> 7; // 255 -> 256
				uint32_t sfactor_g = GPART(src); sfactor_g += sfactor_g >> 7; // 255 -> 256
				uint32_t sfactor_b = BPART(src); sfactor_b += sfactor_b >> 7; // 255 -> 256
				uint32_t sfactor_a = fgalpha; sfactor_a += sfactor_a >> 7; // 255 -> 256
				uint32_t dfactor_r = 256 - sfactor_r;
				uint32_t dfactor_g = 256 - sfactor_g;
				uint32_t dfactor_b = 256 - sfactor_b;
				uint32_t out_r = (RPART(dest) * dfactor_r + RPART(src) * sfactor_r + 128) >> 8;
				uint32_t out_g = (GPART(dest) * dfactor_g + GPART(src) * sfactor_g + 128) >> 8;
				uint32_t out_b = (BPART(dest) * dfactor_b + BPART(src) * sfactor_b + 128) >> 8;

				destLine[x] = RGB256k.All[((out_r >> 2) << 12) | ((out_g >> 2) << 6) | (out_b >> 2)];
			}
			else if (ModeT::BlendSrc == STYLEALPHA_Src && ModeT::BlendDest == STYLEALPHA_InvSrc && fgalpha == 255)
			{
				destLine[x] = shadedfg;
			}
			else if (ModeT::BlendSrc != STYLEALPHA_Src || ModeT::BlendDest != STYLEALPHA_InvSrc || fgalpha != 0)
			{
				uint32_t src = GPalette.BaseColors[shadedfg];
				uint32_t dest = GPalette.BaseColors[destLine[x]];

				uint32_t sfactor = fgalpha; sfactor += sfactor >> 7; // 255 -> 256
				uint32_t dfactor = 256 - sfactor;
				uint32_t src_r = RPART(src) * sfactor;
				uint32_t src_g = GPART(src) * sfactor;
				uint32_t src_b = BPART(src) * sfactor;
				uint32_t dest_r = RPART(dest);
				uint32_t dest_g = GPART(dest);
				uint32_t dest_b = BPART(dest);
				if (ModeT::BlendDest == STYLEALPHA_One)
				{
					dest_r <<= 8;
					dest_g <<= 8;
					dest_b <<= 8;
				}
				else
				{
					uint32_t dfactor = 256 - sfactor;
					dest_r *= dfactor;
					dest_g *= dfactor;
					dest_b *= dfactor;
				}

				uint32_t out_r, out_g, out_b;
				if (ModeT::BlendOp == STYLEOP_Add)
				{
					if (ModeT::BlendDest == STYLEALPHA_One)
					{
						out_r = MIN<int32_t>((dest_r + src_r + 128) >> 8, 255);
						out_g = MIN<int32_t>((dest_g + src_g + 128) >> 8, 255);
						out_b = MIN<int32_t>((dest_b + src_b + 128) >> 8, 255);
					}
					else
					{
						out_r = (dest_r + src_r + 128) >> 8;
						out_g = (dest_g + src_g + 128) >> 8;
						out_b = (dest_b + src_b + 128) >> 8;
					}
				}
				else if (ModeT::BlendOp == STYLEOP_RevSub)
				{
					out_r = MAX<int32_t>(static_cast<int32_t>(dest_r - src_r + 128) >> 8, 0);
					out_g = MAX<int32_t>(static_cast<int32_t>(dest_g - src_g + 128) >> 8, 0);
					out_b = MAX<int32_t>(static_cast<int32_t>(dest_b - src_b + 128) >> 8, 0);
				}
				else //if (ModeT::BlendOp == STYLEOP_Sub)
				{
					out_r = MAX<int32_t>(static_cast<int32_t>(src_r - dest_r + 128) >> 8, 0);
					out_g = MAX<int32_t>(static_cast<int32_t>(src_g - dest_g + 128) >> 8, 0);
					out_b = MAX<int32_t>(static_cast<int32_t>(src_b - dest_b + 128) >> 8, 0);
				}

				destLine[x] = RGB256k.All[((out_r >> 2) << 12) | ((out_g >> 2) << 6) | (out_b >> 2)];
			}
		}

		posW += stepW;
		posU += stepU;
		posV += stepV;
		if (OptT::Flags & SWOPT_DynLights)
		{
			posdynlight_r += dynlights_r[x >> 4];
			posdynlight_g += dynlights_g[x >> 4];
			posdynlight_b += dynlights_b[x >> 4];
		}
		if (!(OptT::Flags & SWOPT_FixedLight))
			lightpos += lightarray[x >> 4];
		x++;
	}
}

template<typename ModeT>
void DrawSpan8(int y, int x0, int x1, const TriDrawTriangleArgs *args)
{
	using namespace TriScreenDrawerModes;

	if (args->uniforms->NumLights() == 0 && args->uniforms->DynLightColor() == 0)
	{
		if (!args->uniforms->FixedLight())
			DrawSpanOpt8<ModeT, DrawerOptC>(y, x0, x1, args);
		else
			DrawSpanOpt8<ModeT, DrawerOptCF>(y, x0, x1, args);
	}
	else
	{
		if (!args->uniforms->FixedLight())
			DrawSpanOpt8<ModeT, DrawerOptLC>(y, x0, x1, args);
		else
			DrawSpanOpt8<ModeT, DrawerOptLCF>(y, x0, x1, args);
	}
}

template<typename ModeT>
void DrawRect8(const void *destOrg, int destWidth, int destHeight, int destPitch, const RectDrawArgs *args, PolyTriangleThreadData *thread)
{
	using namespace TriScreenDrawerModes;

	int x0 = clamp((int)(args->X0() + 0.5f), 0, destWidth);
	int x1 = clamp((int)(args->X1() + 0.5f), 0, destWidth);
	int y0 = clamp((int)(args->Y0() + 0.5f), 0, destHeight);
	int y1 = clamp((int)(args->Y1() + 0.5f), 0, destHeight);

	if (x1 <= x0 || y1 <= y0)
		return;

	const uint8_t *colormaps, *texPixels, *translation;
	int texWidth, texHeight;
	uint32_t fillcolor;
	int alpha;
	uint32_t light;

	texPixels = args->TexturePixels();
	translation = args->Translation();
	texWidth = args->TextureWidth();
	texHeight = args->TextureHeight();
	fillcolor = args->Color();
	alpha = args->Alpha();
	colormaps = args->BaseColormap();
	light = args->Light();
	light += light >> 7; // 255 -> 256
	light = ((256 - light) * NUMCOLORMAPS) & 0xffffff00;

	fixed_t fuzzscale;
	int _fuzzpos;
	if (ModeT::BlendOp == STYLEOP_Fuzz)
	{
		fuzzscale = (200 << FRACBITS) / viewheight;
		_fuzzpos = swrenderer::fuzzpos;
	}

	float fstepU = (args->U1() - args->U0()) / (args->X1() - args->X0());
	float fstepV = (args->V1() - args->V0()) / (args->Y1() - args->Y0());
	uint32_t startU = (int32_t)((args->U0() + (x0 + 0.5f - args->X0()) * fstepU) * 0x1000000);
	uint32_t startV = (int32_t)((args->V0() + (y0 + 0.5f - args->Y0()) * fstepV) * 0x1000000);
	uint32_t stepU = (int32_t)(fstepU * 0x1000000);
	uint32_t stepV = (int32_t)(fstepV * 0x1000000);

	uint32_t posV = startV;
	int num_cores = thread->num_cores;
	int skip = thread->skipped_by_thread(y0);
	posV += skip * stepV;
	stepV *= num_cores;
	for (int y = y0 + skip; y < y1; y += num_cores, posV += stepV)
	{
		uint8_t *destLine = ((uint8_t*)destOrg) + y * destPitch;

		uint32_t posU = startU;
		for (int x = x0; x < x1; x++)
		{
			if (ModeT::BlendOp == STYLEOP_Fuzz)
			{
				using namespace swrenderer;

				uint32_t texelX = (((posU << 8) >> 16) * texWidth) >> 16;
				uint32_t texelY = (((posV << 8) >> 16) * texHeight) >> 16;
				unsigned int sampleshadeout = (texPixels[texelX * texHeight + texelY] != 0) ? 256 : 0;

				int scaled_x = (x * fuzzscale) >> FRACBITS;
				int fuzz_x = fuzz_random_x_offset[scaled_x % FUZZ_RANDOM_X_SIZE] + _fuzzpos;

				fixed_t fuzzcount = FUZZTABLE << FRACBITS;
				fixed_t fuzz = ((fuzz_x << FRACBITS) + y * fuzzscale) % fuzzcount;
				unsigned int alpha = fuzzoffset[fuzz >> FRACBITS];

				sampleshadeout = (sampleshadeout * alpha) >> 5;

				uint32_t a = 256 - sampleshadeout;

				uint32_t dest = GPalette.BaseColors[destLine[x]].d;
				uint32_t r = (RPART(dest) * a) >> 8;
				uint32_t g = (GPART(dest) * a) >> 8;
				uint32_t b = (BPART(dest) * a) >> 8;
				destLine[x] = RGB256k.All[((r >> 2) << 12) | ((g >> 2) << 6) | (b >> 2)];
			}
			else
			{
				int fg = 0;
				if (ModeT::SWFlags & SWSTYLEF_Fill)
				{
					fg = fillcolor;
				}
				else
				{
					uint32_t texelX = (((posU << 8) >> 16) * texWidth) >> 16;
					uint32_t texelY = (((posV << 8) >> 16) * texHeight) >> 16;
					fg = texPixels[texelX * texHeight + texelY];
				}

				int fgalpha = 255;

				if (ModeT::BlendDest == STYLEALPHA_InvSrc)
				{
					if (fg == 0)
						fgalpha = 0;
				}

				if ((ModeT::Flags & STYLEF_ColorIsFixed) && !(ModeT::SWFlags & SWSTYLEF_Fill))
				{
					if (ModeT::Flags & STYLEF_RedIsAlpha)
						fgalpha = fg;
					fg = fillcolor;
				}

				if (!(ModeT::Flags & STYLEF_Alpha1))
				{
					fgalpha = (fgalpha * alpha) >> 8;
				}

				if (ModeT::SWFlags & SWSTYLEF_Translated)
					fg = translation[fg];

				uint8_t shadedfg = colormaps[light + fg];

				if (ModeT::BlendSrc == STYLEALPHA_One && ModeT::BlendDest == STYLEALPHA_Zero)
				{
					destLine[x] = shadedfg;
				}
				else if (ModeT::BlendSrc == STYLEALPHA_One && ModeT::BlendDest == STYLEALPHA_One)
				{
					uint32_t src = GPalette.BaseColors[shadedfg];
					uint32_t dest = GPalette.BaseColors[destLine[x]];

					if (ModeT::BlendOp == STYLEOP_Add)
					{
						uint32_t out_r = MIN<uint32_t>(RPART(dest) + RPART(src), 255);
						uint32_t out_g = MIN<uint32_t>(GPART(dest) + GPART(src), 255);
						uint32_t out_b = MIN<uint32_t>(BPART(dest) + BPART(src), 255);
						destLine[x] = RGB256k.All[((out_r >> 2) << 12) | ((out_g >> 2) << 6) | (out_b >> 2)];
					}
					else if (ModeT::BlendOp == STYLEOP_RevSub)
					{
						uint32_t out_r = MAX<uint32_t>(RPART(dest) - RPART(src), 0);
						uint32_t out_g = MAX<uint32_t>(GPART(dest) - GPART(src), 0);
						uint32_t out_b = MAX<uint32_t>(BPART(dest) - BPART(src), 0);
						destLine[x] = RGB256k.All[((out_r >> 2) << 12) | ((out_g >> 2) << 6) | (out_b >> 2)];
					}
					else //if (ModeT::BlendOp == STYLEOP_Sub)
					{
						uint32_t out_r = MAX<uint32_t>(RPART(src) - RPART(dest), 0);
						uint32_t out_g = MAX<uint32_t>(GPART(src) - GPART(dest), 0);
						uint32_t out_b = MAX<uint32_t>(BPART(src) - BPART(dest), 0);
						destLine[x] = RGB256k.All[((out_r >> 2) << 12) | ((out_g >> 2) << 6) | (out_b >> 2)];
					}
				}
				else if (ModeT::SWFlags & SWSTYLEF_SrcColorOneMinusSrcColor)
				{
					uint32_t src = GPalette.BaseColors[shadedfg];
					uint32_t dest = GPalette.BaseColors[destLine[x]];

					uint32_t sfactor_r = RPART(src); sfactor_r += sfactor_r >> 7; // 255 -> 256
					uint32_t sfactor_g = GPART(src); sfactor_g += sfactor_g >> 7; // 255 -> 256
					uint32_t sfactor_b = BPART(src); sfactor_b += sfactor_b >> 7; // 255 -> 256
					uint32_t sfactor_a = fgalpha; sfactor_a += sfactor_a >> 7; // 255 -> 256
					uint32_t dfactor_r = 256 - sfactor_r;
					uint32_t dfactor_g = 256 - sfactor_g;
					uint32_t dfactor_b = 256 - sfactor_b;
					uint32_t out_r = (RPART(dest) * dfactor_r + RPART(src) * sfactor_r + 128) >> 8;
					uint32_t out_g = (GPART(dest) * dfactor_g + GPART(src) * sfactor_g + 128) >> 8;
					uint32_t out_b = (BPART(dest) * dfactor_b + BPART(src) * sfactor_b + 128) >> 8;

					destLine[x] = RGB256k.All[((out_r >> 2) << 12) | ((out_g >> 2) << 6) | (out_b >> 2)];
				}
				else if (ModeT::BlendSrc == STYLEALPHA_Src && ModeT::BlendDest == STYLEALPHA_InvSrc && fgalpha == 255)
				{
					destLine[x] = shadedfg;
				}
				else if (ModeT::BlendSrc != STYLEALPHA_Src || ModeT::BlendDest != STYLEALPHA_InvSrc || fgalpha != 0)
				{
					uint32_t src = GPalette.BaseColors[shadedfg];
					uint32_t dest = GPalette.BaseColors[destLine[x]];

					uint32_t sfactor = fgalpha; sfactor += sfactor >> 7; // 255 -> 256
					uint32_t dfactor = 256 - sfactor;
					uint32_t src_r = RPART(src) * sfactor;
					uint32_t src_g = GPART(src) * sfactor;
					uint32_t src_b = BPART(src) * sfactor;
					uint32_t dest_r = RPART(dest);
					uint32_t dest_g = GPART(dest);
					uint32_t dest_b = BPART(dest);
					if (ModeT::BlendDest == STYLEALPHA_One)
					{
						dest_r <<= 8;
						dest_g <<= 8;
						dest_b <<= 8;
					}
					else
					{
						uint32_t dfactor = 256 - sfactor;
						dest_r *= dfactor;
						dest_g *= dfactor;
						dest_b *= dfactor;
					}

					uint32_t out_r, out_g, out_b;
					if (ModeT::BlendOp == STYLEOP_Add)
					{
						if (ModeT::BlendDest == STYLEALPHA_One)
						{
							out_r = MIN<int32_t>((dest_r + src_r + 128) >> 8, 255);
							out_g = MIN<int32_t>((dest_g + src_g + 128) >> 8, 255);
							out_b = MIN<int32_t>((dest_b + src_b + 128) >> 8, 255);
						}
						else
						{
							out_r = (dest_r + src_r + 128) >> 8;
							out_g = (dest_g + src_g + 128) >> 8;
							out_b = (dest_b + src_b + 128) >> 8;
						}
					}
					else if (ModeT::BlendOp == STYLEOP_RevSub)
					{
						out_r = MAX<int32_t>(static_cast<int32_t>(dest_r - src_r + 128) >> 8, 0);
						out_g = MAX<int32_t>(static_cast<int32_t>(dest_g - src_g + 128) >> 8, 0);
						out_b = MAX<int32_t>(static_cast<int32_t>(dest_b - src_b + 128) >> 8, 0);
					}
					else //if (ModeT::BlendOp == STYLEOP_Sub)
					{
						out_r = MAX<int32_t>(static_cast<int32_t>(src_r - dest_r + 128) >> 8, 0);
						out_g = MAX<int32_t>(static_cast<int32_t>(src_g - dest_g + 128) >> 8, 0);
						out_b = MAX<int32_t>(static_cast<int32_t>(src_b - dest_b + 128) >> 8, 0);
					}

					destLine[x] = RGB256k.All[((out_r >> 2) << 12) | ((out_g >> 2) << 6) | (out_b >> 2)];
				}
			}

			posU += stepU;
		}
	}
}

template<typename ModeT, typename OptT>
void DrawRectOpt32(const void *destOrg, int destWidth, int destHeight, int destPitch, const RectDrawArgs *args, PolyTriangleThreadData *thread)
{
	using namespace TriScreenDrawerModes;

	int x0 = clamp((int)(args->X0() + 0.5f), 0, destWidth);
	int x1 = clamp((int)(args->X1() + 0.5f), 0, destWidth);
	int y0 = clamp((int)(args->Y0() + 0.5f), 0, destHeight);
	int y1 = clamp((int)(args->Y1() + 0.5f), 0, destHeight);

	if (x1 <= x0 || y1 <= y0)
		return;

	const uint32_t *texPixels, *translation;
	int texWidth, texHeight;
	uint32_t fillcolor;
	int alpha;
	uint32_t light;
	uint32_t shade_fade_r, shade_fade_g, shade_fade_b, shade_light_r, shade_light_g, shade_light_b, desaturate, inv_desaturate;

	texPixels = (const uint32_t*)args->TexturePixels();
	translation = (const uint32_t*)args->Translation();
	texWidth = args->TextureWidth();
	texHeight = args->TextureHeight();
	fillcolor = args->Color();
	alpha = args->Alpha();
	light = args->Light();
	light += light >> 7; // 255 -> 256

	if (OptT::Flags & SWOPT_ColoredFog)
	{
		shade_fade_r = args->ShadeFadeRed();
		shade_fade_g = args->ShadeFadeGreen();
		shade_fade_b = args->ShadeFadeBlue();
		shade_light_r = args->ShadeLightRed();
		shade_light_g = args->ShadeLightGreen();
		shade_light_b = args->ShadeLightBlue();
		desaturate = args->ShadeDesaturate();
		inv_desaturate = 256 - desaturate;
	}

	fixed_t fuzzscale;
	int _fuzzpos;
	if (ModeT::BlendOp == STYLEOP_Fuzz)
	{
		fuzzscale = (200 << FRACBITS) / viewheight;
		_fuzzpos = swrenderer::fuzzpos;
	}

	float fstepU = (args->U1() - args->U0()) / (args->X1() - args->X0());
	float fstepV = (args->V1() - args->V0()) / (args->Y1() - args->Y0());
	uint32_t startU = (int32_t)((args->U0() + (x0 + 0.5f - args->X0()) * fstepU) * 0x1000000);
	uint32_t startV = (int32_t)((args->V0() + (y0 + 0.5f - args->Y0()) * fstepV) * 0x1000000);
	uint32_t stepU = (int32_t)(fstepU * 0x1000000);
	uint32_t stepV = (int32_t)(fstepV * 0x1000000);

	uint32_t posV = startV;
	int num_cores = thread->num_cores;
	int skip = thread->skipped_by_thread(y0);
	posV += skip * stepV;
	stepV *= num_cores;
	for (int y = y0 + skip; y < y1; y += num_cores, posV += stepV)
	{
		uint32_t *destLine = ((uint32_t*)destOrg) + y * destPitch;

		uint32_t posU = startU;
		for (int x = x0; x < x1; x++)
		{
			if (ModeT::BlendOp == STYLEOP_Fuzz)
			{
				using namespace swrenderer;

				uint32_t texelX = (((posU << 8) >> 16) * texWidth) >> 16;
				uint32_t texelY = (((posV << 8) >> 16) * texHeight) >> 16;
				unsigned int sampleshadeout = APART(texPixels[texelX * texHeight + texelY]);
				sampleshadeout += sampleshadeout >> 7; // 255 -> 256

				int scaled_x = (x * fuzzscale) >> FRACBITS;
				int fuzz_x = fuzz_random_x_offset[scaled_x % FUZZ_RANDOM_X_SIZE] + _fuzzpos;

				fixed_t fuzzcount = FUZZTABLE << FRACBITS;
				fixed_t fuzz = ((fuzz_x << FRACBITS) + y * fuzzscale) % fuzzcount;
				unsigned int alpha = fuzzoffset[fuzz >> FRACBITS];

				sampleshadeout = (sampleshadeout * alpha) >> 5;

				uint32_t a = 256 - sampleshadeout;

				uint32_t dest = destLine[x];
				uint32_t out_r = (RPART(dest) * a) >> 8;
				uint32_t out_g = (GPART(dest) * a) >> 8;
				uint32_t out_b = (BPART(dest) * a) >> 8;
				destLine[x] = MAKEARGB(255, out_r, out_g, out_b);
			}
			else
			{
				uint32_t fg = 0;

				if (ModeT::SWFlags & SWSTYLEF_Fill)
				{
					fg = fillcolor;
				}
				else if (ModeT::SWFlags & SWSTYLEF_FogBoundary)
				{
					fg = destLine[x];
				}
				else
				{
					uint32_t texelX = (((posU << 8) >> 16) * texWidth) >> 16;
					uint32_t texelY = (((posV << 8) >> 16) * texHeight) >> 16;

					if (ModeT::SWFlags & SWSTYLEF_Translated)
					{
						fg = translation[((const uint8_t*)texPixels)[texelX * texHeight + texelY]];
					}
					else if (ModeT::Flags & STYLEF_RedIsAlpha)
					{
						fg = ((const uint8_t*)texPixels)[texelX * texHeight + texelY];
					}
					else
					{
						fg = texPixels[texelX * texHeight + texelY];
					}
				}

				if ((ModeT::Flags & STYLEF_ColorIsFixed) && !(ModeT::SWFlags & SWSTYLEF_Fill))
				{
					if (ModeT::Flags & STYLEF_RedIsAlpha)
						fg = (fg << 24) | (fillcolor & 0x00ffffff);
					else
						fg = (fg & 0xff000000) | (fillcolor & 0x00ffffff);
				}

				uint32_t fgalpha = fg >> 24;

				if (!(ModeT::Flags & STYLEF_Alpha1))
				{
					fgalpha = (fgalpha * alpha) >> 8;
				}

				int lightshade = light;

				uint32_t lit_r = 0, lit_g = 0, lit_b = 0;

				uint32_t shadedfg_r, shadedfg_g, shadedfg_b;
				if (OptT::Flags & SWOPT_ColoredFog)
				{
					uint32_t fg_r = RPART(fg);
					uint32_t fg_g = GPART(fg);
					uint32_t fg_b = BPART(fg);
					uint32_t intensity = ((fg_r * 77 + fg_g * 143 + fg_b * 37) >> 8) * desaturate;
					shadedfg_r = (((shade_fade_r + ((fg_r * inv_desaturate + intensity) >> 8) * lightshade) >> 8) * shade_light_r) >> 8;
					shadedfg_g = (((shade_fade_g + ((fg_g * inv_desaturate + intensity) >> 8) * lightshade) >> 8) * shade_light_g) >> 8;
					shadedfg_b = (((shade_fade_b + ((fg_b * inv_desaturate + intensity) >> 8) * lightshade) >> 8) * shade_light_b) >> 8;
				}
				else
				{
					shadedfg_r = (RPART(fg) * lightshade) >> 8;
					shadedfg_g = (GPART(fg) * lightshade) >> 8;
					shadedfg_b = (BPART(fg) * lightshade) >> 8;
				}

				if (ModeT::BlendSrc == STYLEALPHA_One && ModeT::BlendDest == STYLEALPHA_Zero)
				{
					destLine[x] = MAKEARGB(255, shadedfg_r, shadedfg_g, shadedfg_b);
				}
				else if (ModeT::BlendSrc == STYLEALPHA_One && ModeT::BlendDest == STYLEALPHA_One)
				{
					uint32_t dest = destLine[x];

					if (ModeT::BlendOp == STYLEOP_Add)
					{
						uint32_t out_r = MIN<uint32_t>(RPART(dest) + shadedfg_r, 255);
						uint32_t out_g = MIN<uint32_t>(GPART(dest) + shadedfg_g, 255);
						uint32_t out_b = MIN<uint32_t>(BPART(dest) + shadedfg_b, 255);
						destLine[x] = MAKEARGB(255, out_r, out_g, out_b);
					}
					else if (ModeT::BlendOp == STYLEOP_RevSub)
					{
						uint32_t out_r = MAX<uint32_t>(RPART(dest) - shadedfg_r, 0);
						uint32_t out_g = MAX<uint32_t>(GPART(dest) - shadedfg_g, 0);
						uint32_t out_b = MAX<uint32_t>(BPART(dest) - shadedfg_b, 0);
						destLine[x] = MAKEARGB(255, out_r, out_g, out_b);
					}
					else //if (ModeT::BlendOp == STYLEOP_Sub)
					{
						uint32_t out_r = MAX<uint32_t>(shadedfg_r - RPART(dest), 0);
						uint32_t out_g = MAX<uint32_t>(shadedfg_g - GPART(dest), 0);
						uint32_t out_b = MAX<uint32_t>(shadedfg_b - BPART(dest), 0);
						destLine[x] = MAKEARGB(255, out_r, out_g, out_b);
					}
				}
				else if (ModeT::SWFlags & SWSTYLEF_SrcColorOneMinusSrcColor)
				{
					uint32_t dest = destLine[x];

					uint32_t sfactor_r = shadedfg_r; sfactor_r += sfactor_r >> 7; // 255 -> 256
					uint32_t sfactor_g = shadedfg_g; sfactor_g += sfactor_g >> 7; // 255 -> 256
					uint32_t sfactor_b = shadedfg_b; sfactor_b += sfactor_b >> 7; // 255 -> 256
					uint32_t sfactor_a = fgalpha; sfactor_a += sfactor_a >> 7; // 255 -> 256
					uint32_t dfactor_r = 256 - sfactor_r;
					uint32_t dfactor_g = 256 - sfactor_g;
					uint32_t dfactor_b = 256 - sfactor_b;
					uint32_t out_r = (RPART(dest) * dfactor_r + shadedfg_r * sfactor_r + 128) >> 8;
					uint32_t out_g = (GPART(dest) * dfactor_g + shadedfg_g * sfactor_g + 128) >> 8;
					uint32_t out_b = (BPART(dest) * dfactor_b + shadedfg_b * sfactor_b + 128) >> 8;

					destLine[x] = MAKEARGB(255, out_r, out_g, out_b);
				}
				else if (ModeT::BlendSrc == STYLEALPHA_Src && ModeT::BlendDest == STYLEALPHA_InvSrc && fgalpha == 255)
				{
					destLine[x] = MAKEARGB(255, shadedfg_r, shadedfg_g, shadedfg_b);
				}
				else if (ModeT::BlendSrc != STYLEALPHA_Src || ModeT::BlendDest != STYLEALPHA_InvSrc || fgalpha != 0)
				{
					uint32_t dest = destLine[x];

					uint32_t sfactor = fgalpha; sfactor += sfactor >> 7; // 255 -> 256
					uint32_t src_r = shadedfg_r * sfactor;
					uint32_t src_g = shadedfg_g * sfactor;
					uint32_t src_b = shadedfg_b * sfactor;
					uint32_t dest_r = RPART(dest);
					uint32_t dest_g = GPART(dest);
					uint32_t dest_b = BPART(dest);
					if (ModeT::BlendDest == STYLEALPHA_One)
					{
						dest_r <<= 8;
						dest_g <<= 8;
						dest_b <<= 8;
					}
					else
					{
						uint32_t dfactor = 256 - sfactor;
						dest_r *= dfactor;
						dest_g *= dfactor;
						dest_b *= dfactor;
					}

					uint32_t out_r, out_g, out_b;
					if (ModeT::BlendOp == STYLEOP_Add)
					{
						if (ModeT::BlendDest == STYLEALPHA_One)
						{
							out_r = MIN<int32_t>((dest_r + src_r + 128) >> 8, 255);
							out_g = MIN<int32_t>((dest_g + src_g + 128) >> 8, 255);
							out_b = MIN<int32_t>((dest_b + src_b + 128) >> 8, 255);
						}
						else
						{
							out_r = (dest_r + src_r + 128) >> 8;
							out_g = (dest_g + src_g + 128) >> 8;
							out_b = (dest_b + src_b + 128) >> 8;
						}
					}
					else if (ModeT::BlendOp == STYLEOP_RevSub)
					{
						out_r = MAX<int32_t>(static_cast<int32_t>(dest_r - src_r + 128) >> 8, 0);
						out_g = MAX<int32_t>(static_cast<int32_t>(dest_g - src_g + 128) >> 8, 0);
						out_b = MAX<int32_t>(static_cast<int32_t>(dest_b - src_b + 128) >> 8, 0);
					}
					else //if (ModeT::BlendOp == STYLEOP_Sub)
					{
						out_r = MAX<int32_t>(static_cast<int32_t>(src_r - dest_r + 128) >> 8, 0);
						out_g = MAX<int32_t>(static_cast<int32_t>(src_g - dest_g + 128) >> 8, 0);
						out_b = MAX<int32_t>(static_cast<int32_t>(src_b - dest_b + 128) >> 8, 0);
					}

					destLine[x] = MAKEARGB(255, out_r, out_g, out_b);
				}
			}

			posU += stepU;
		}
	}
}

template<typename ModeT>
void DrawRect32(const void *destOrg, int destWidth, int destHeight, int destPitch, const RectDrawArgs *args, PolyTriangleThreadData *thread)
{
	using namespace TriScreenDrawerModes;

	if (args->SimpleShade())
		DrawRectOpt32<ModeT, DrawerOptF>(destOrg, destWidth, destHeight, destPitch, args, thread);
	else
		DrawRectOpt32<ModeT, DrawerOptCF>(destOrg, destWidth, destHeight, destPitch, args, thread);
}

void(*ScreenTriangle::SpanDrawers8[])(int, int, int, const TriDrawTriangleArgs *) =
{
	&DrawSpan8<TriScreenDrawerModes::StyleOpaque>,
	&DrawSpan8<TriScreenDrawerModes::StyleSkycap>,
	&DrawSpan8<TriScreenDrawerModes::StyleFogBoundary>,
	&DrawSpan8<TriScreenDrawerModes::StyleSrcColor>,
	&DrawSpan8<TriScreenDrawerModes::StyleFill>,
	&DrawSpan8<TriScreenDrawerModes::StyleNormal>,
	&DrawSpan8<TriScreenDrawerModes::StyleFuzzy>,
	&DrawSpan8<TriScreenDrawerModes::StyleStencil>,
	&DrawSpan8<TriScreenDrawerModes::StyleTranslucent>,
	&DrawSpan8<TriScreenDrawerModes::StyleAdd>,
	&DrawSpan8<TriScreenDrawerModes::StyleShaded>,
	&DrawSpan8<TriScreenDrawerModes::StyleTranslucentStencil>,
	&DrawSpan8<TriScreenDrawerModes::StyleShadow>,
	&DrawSpan8<TriScreenDrawerModes::StyleSubtract>,
	&DrawSpan8<TriScreenDrawerModes::StyleAddStencil>,
	&DrawSpan8<TriScreenDrawerModes::StyleAddShaded>,
	&DrawSpan8<TriScreenDrawerModes::StyleOpaqueTranslated>,
	&DrawSpan8<TriScreenDrawerModes::StyleSrcColorTranslated>,
	&DrawSpan8<TriScreenDrawerModes::StyleNormalTranslated>,
	&DrawSpan8<TriScreenDrawerModes::StyleStencilTranslated>,
	&DrawSpan8<TriScreenDrawerModes::StyleTranslucentTranslated>,
	&DrawSpan8<TriScreenDrawerModes::StyleAddTranslated>,
	&DrawSpan8<TriScreenDrawerModes::StyleShadedTranslated>,
	&DrawSpan8<TriScreenDrawerModes::StyleTranslucentStencilTranslated>,
	&DrawSpan8<TriScreenDrawerModes::StyleShadowTranslated>,
	&DrawSpan8<TriScreenDrawerModes::StyleSubtractTranslated>,
	&DrawSpan8<TriScreenDrawerModes::StyleAddStencilTranslated>,
	&DrawSpan8<TriScreenDrawerModes::StyleAddShadedTranslated>
};

void(*ScreenTriangle::SpanDrawers32[])(int, int, int, const TriDrawTriangleArgs *) =
{
	&DrawSpan32<TriScreenDrawerModes::StyleOpaque>,
	&DrawSpan32<TriScreenDrawerModes::StyleSkycap>,
	&DrawSpan32<TriScreenDrawerModes::StyleFogBoundary>,
	&DrawSpan32<TriScreenDrawerModes::StyleSrcColor>,
	&DrawSpan32<TriScreenDrawerModes::StyleFill>,
	&DrawSpan32<TriScreenDrawerModes::StyleNormal>,
	&DrawSpan32<TriScreenDrawerModes::StyleFuzzy>,
	&DrawSpan32<TriScreenDrawerModes::StyleStencil>,
	&DrawSpan32<TriScreenDrawerModes::StyleTranslucent>,
	&DrawSpan32<TriScreenDrawerModes::StyleAdd>,
	&DrawSpan32<TriScreenDrawerModes::StyleShaded>,
	&DrawSpan32<TriScreenDrawerModes::StyleTranslucentStencil>,
	&DrawSpan32<TriScreenDrawerModes::StyleShadow>,
	&DrawSpan32<TriScreenDrawerModes::StyleSubtract>,
	&DrawSpan32<TriScreenDrawerModes::StyleAddStencil>,
	&DrawSpan32<TriScreenDrawerModes::StyleAddShaded>,
	&DrawSpan32<TriScreenDrawerModes::StyleOpaqueTranslated>,
	&DrawSpan32<TriScreenDrawerModes::StyleSrcColorTranslated>,
	&DrawSpan32<TriScreenDrawerModes::StyleNormalTranslated>,
	&DrawSpan32<TriScreenDrawerModes::StyleStencilTranslated>,
	&DrawSpan32<TriScreenDrawerModes::StyleTranslucentTranslated>,
	&DrawSpan32<TriScreenDrawerModes::StyleAddTranslated>,
	&DrawSpan32<TriScreenDrawerModes::StyleShadedTranslated>,
	&DrawSpan32<TriScreenDrawerModes::StyleTranslucentStencilTranslated>,
	&DrawSpan32<TriScreenDrawerModes::StyleShadowTranslated>,
	&DrawSpan32<TriScreenDrawerModes::StyleSubtractTranslated>,
	&DrawSpan32<TriScreenDrawerModes::StyleAddStencilTranslated>,
	&DrawSpan32<TriScreenDrawerModes::StyleAddShadedTranslated>
};

void(*ScreenTriangle::RectDrawers8[])(const void *, int, int, int, const RectDrawArgs *, PolyTriangleThreadData *) =
{
	&DrawRect8<TriScreenDrawerModes::StyleOpaque>,
	&DrawRect8<TriScreenDrawerModes::StyleSkycap>,
	&DrawRect8<TriScreenDrawerModes::StyleFogBoundary>,
	&DrawRect8<TriScreenDrawerModes::StyleSrcColor>,
	&DrawRect8<TriScreenDrawerModes::StyleFill>,
	&DrawRect8<TriScreenDrawerModes::StyleNormal>,
	&DrawRect8<TriScreenDrawerModes::StyleFuzzy>,
	&DrawRect8<TriScreenDrawerModes::StyleStencil>,
	&DrawRect8<TriScreenDrawerModes::StyleTranslucent>,
	&DrawRect8<TriScreenDrawerModes::StyleAdd>,
	&DrawRect8<TriScreenDrawerModes::StyleShaded>,
	&DrawRect8<TriScreenDrawerModes::StyleTranslucentStencil>,
	&DrawRect8<TriScreenDrawerModes::StyleShadow>,
	&DrawRect8<TriScreenDrawerModes::StyleSubtract>,
	&DrawRect8<TriScreenDrawerModes::StyleAddStencil>,
	&DrawRect8<TriScreenDrawerModes::StyleAddShaded>,
	&DrawRect8<TriScreenDrawerModes::StyleOpaqueTranslated>,
	&DrawRect8<TriScreenDrawerModes::StyleSrcColorTranslated>,
	&DrawRect8<TriScreenDrawerModes::StyleNormalTranslated>,
	&DrawRect8<TriScreenDrawerModes::StyleStencilTranslated>,
	&DrawRect8<TriScreenDrawerModes::StyleTranslucentTranslated>,
	&DrawRect8<TriScreenDrawerModes::StyleAddTranslated>,
	&DrawRect8<TriScreenDrawerModes::StyleShadedTranslated>,
	&DrawRect8<TriScreenDrawerModes::StyleTranslucentStencilTranslated>,
	&DrawRect8<TriScreenDrawerModes::StyleShadowTranslated>,
	&DrawRect8<TriScreenDrawerModes::StyleSubtractTranslated>,
	&DrawRect8<TriScreenDrawerModes::StyleAddStencilTranslated>,
	&DrawRect8<TriScreenDrawerModes::StyleAddShadedTranslated>
};

void(*ScreenTriangle::RectDrawers32[])(const void *, int, int, int, const RectDrawArgs *, PolyTriangleThreadData *) =
{
	&DrawRect32<TriScreenDrawerModes::StyleOpaque>,
	&DrawRect32<TriScreenDrawerModes::StyleSkycap>,
	&DrawRect32<TriScreenDrawerModes::StyleFogBoundary>,
	&DrawRect32<TriScreenDrawerModes::StyleSrcColor>,
	&DrawRect32<TriScreenDrawerModes::StyleFill>,
	&DrawRect32<TriScreenDrawerModes::StyleNormal>,
	&DrawRect32<TriScreenDrawerModes::StyleFuzzy>,
	&DrawRect32<TriScreenDrawerModes::StyleStencil>,
	&DrawRect32<TriScreenDrawerModes::StyleTranslucent>,
	&DrawRect32<TriScreenDrawerModes::StyleAdd>,
	&DrawRect32<TriScreenDrawerModes::StyleShaded>,
	&DrawRect32<TriScreenDrawerModes::StyleTranslucentStencil>,
	&DrawRect32<TriScreenDrawerModes::StyleShadow>,
	&DrawRect32<TriScreenDrawerModes::StyleSubtract>,
	&DrawRect32<TriScreenDrawerModes::StyleAddStencil>,
	&DrawRect32<TriScreenDrawerModes::StyleAddShaded>,
	&DrawRect32<TriScreenDrawerModes::StyleOpaqueTranslated>,
	&DrawRect32<TriScreenDrawerModes::StyleSrcColorTranslated>,
	&DrawRect32<TriScreenDrawerModes::StyleNormalTranslated>,
	&DrawRect32<TriScreenDrawerModes::StyleStencilTranslated>,
	&DrawRect32<TriScreenDrawerModes::StyleTranslucentTranslated>,
	&DrawRect32<TriScreenDrawerModes::StyleAddTranslated>,
	&DrawRect32<TriScreenDrawerModes::StyleShadedTranslated>,
	&DrawRect32<TriScreenDrawerModes::StyleTranslucentStencilTranslated>,
	&DrawRect32<TriScreenDrawerModes::StyleShadowTranslated>,
	&DrawRect32<TriScreenDrawerModes::StyleSubtractTranslated>,
	&DrawRect32<TriScreenDrawerModes::StyleAddStencilTranslated>,
	&DrawRect32<TriScreenDrawerModes::StyleAddShadedTranslated>
};

int ScreenTriangle::FuzzStart = 0;
