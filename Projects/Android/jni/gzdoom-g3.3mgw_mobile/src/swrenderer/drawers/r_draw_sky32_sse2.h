/*
**  Drawer commands for spans
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

#include "swrenderer/drawers/r_draw_rgba.h"
#include "swrenderer/viewport/r_skydrawer.h"

namespace swrenderer
{
	class DrawSkySingle32Command : public DrawerCommand
	{
	protected:
		SkyDrawerArgs args;
		
	public:
		DrawSkySingle32Command(const SkyDrawerArgs &args) : args(args) { }
		
		void Execute(DrawerThread *thread) override
		{
			uint32_t *dest = (uint32_t *)args.Dest();
			int count = args.Count();
			int pitch = args.Viewport()->RenderTarget->GetPitch();
			const uint32_t *source0 = (const uint32_t *)args.FrontTexturePixels();
			int textureheight0 = args.FrontTextureHeight();

			int32_t frac = args.TextureVPos();
			int32_t fracstep = args.TextureVStep();
			
			uint32_t solid_top = args.SolidTopColor();
			uint32_t solid_bottom = args.SolidBottomColor();
			bool fadeSky = args.FadeSky();

			// Find bands for top solid color, top fade, center textured, bottom fade, bottom solid color:
			int start_fade = 2; // How fast it should fade out
			int fade_length = (1 << (24 - start_fade));
			int start_fadetop_y = (-frac) / fracstep;
			int end_fadetop_y = (fade_length - frac) / fracstep;
			int start_fadebottom_y = ((2 << 24) - fade_length - frac) / fracstep;
			int end_fadebottom_y = ((2 << 24) - frac) / fracstep;
			start_fadetop_y = clamp(start_fadetop_y, 0, count);
			end_fadetop_y = clamp(end_fadetop_y, 0, count);
			start_fadebottom_y = clamp(start_fadebottom_y, 0, count);
			end_fadebottom_y = clamp(end_fadebottom_y, 0, count);

			int num_cores = thread->num_cores;
			int skipped = thread->skipped_by_thread(args.DestY());
			dest = thread->dest_for_thread(args.DestY(), pitch, dest);
			frac += fracstep * skipped;
			fracstep *= num_cores;
			pitch *= num_cores;

			if (!fadeSky)
			{
				count = thread->count_for_thread(args.DestY(), count);

				for (int index = 0; index < count; index++)
				{
					uint32_t sample_index = (((((uint32_t)frac) << 8) >> FRACBITS) * textureheight0) >> FRACBITS;
					*dest = source0[sample_index];
					dest += pitch;
					frac += fracstep;
				}

				return;
			}

			__m128i solid_top_fill = _mm_unpacklo_epi8(_mm_cvtsi32_si128(solid_top), _mm_setzero_si128());
			__m128i solid_bottom_fill = _mm_unpacklo_epi8(_mm_cvtsi32_si128(solid_bottom), _mm_setzero_si128());

			int index = skipped;

			// Top solid color:
			while (index < start_fadetop_y)
			{
				*dest = solid_top;
				dest += pitch;
				frac += fracstep;
				index += num_cores;
			}

			// Top fade:
			while (index < end_fadetop_y)
			{
				uint32_t sample_index = (((((uint32_t)frac) << 8) >> FRACBITS) * textureheight0) >> FRACBITS;
				uint32_t fg = source0[sample_index];

				__m128i alpha = _mm_set1_epi16(MAX(MIN(frac >> (16 - start_fade), 256), 0));
				__m128i inv_alpha = _mm_sub_epi16(_mm_set1_epi16(256), alpha);
				
				__m128i c = _mm_unpacklo_epi8(_mm_cvtsi32_si128(fg), _mm_setzero_si128());
				c = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(c, alpha), _mm_mullo_epi16(solid_top_fill, inv_alpha)), 8);
				*dest = _mm_cvtsi128_si32(_mm_packus_epi16(c, _mm_setzero_si128()));

				frac += fracstep;
				dest += pitch;
				index += num_cores;
			}

			// Textured center:
			while (index < start_fadebottom_y)
			{
				uint32_t sample_index = (((((uint32_t)frac) << 8) >> FRACBITS) * textureheight0) >> FRACBITS;
				*dest = source0[sample_index];

				frac += fracstep;
				dest += pitch;
				index += num_cores;
			}

			// Fade bottom:
			while (index < end_fadebottom_y)
			{
				uint32_t sample_index = (((((uint32_t)frac) << 8) >> FRACBITS) * textureheight0) >> FRACBITS;
				uint32_t fg = source0[sample_index];

				__m128i alpha = _mm_set1_epi16(MAX(MIN(((2 << 24) - frac) >> (16 - start_fade), 256), 0));
				__m128i inv_alpha = _mm_sub_epi16(_mm_set1_epi16(256), alpha);
				
				__m128i c = _mm_unpacklo_epi8(_mm_cvtsi32_si128(fg), _mm_setzero_si128());
				c = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(c, alpha), _mm_mullo_epi16(solid_top_fill, inv_alpha)), 8);
				*dest = _mm_cvtsi128_si32(_mm_packus_epi16(c, _mm_setzero_si128()));

				frac += fracstep;
				dest += pitch;
				index += num_cores;
			}

			// Bottom solid color:
			while (index < count)
			{
				*dest = solid_bottom;
				dest += pitch;
				index += num_cores;
			}
		}
	};
	
	class DrawSkyDouble32Command : public DrawerCommand
	{
	protected:
		SkyDrawerArgs args;
		
	public:
		DrawSkyDouble32Command(const SkyDrawerArgs &args) : args(args) { }
		
		void Execute(DrawerThread *thread) override
		{
			uint32_t *dest = (uint32_t *)args.Dest();
			int count = args.Count();
			int pitch = args.Viewport()->RenderTarget->GetPitch();
			const uint32_t *source0 = (const uint32_t *)args.FrontTexturePixels();
			const uint32_t *source1 = (const uint32_t *)args.BackTexturePixels();
			int textureheight0 = args.FrontTextureHeight();
			uint32_t maxtextureheight1 = args.BackTextureHeight() - 1;

			int32_t frac = args.TextureVPos();
			int32_t fracstep = args.TextureVStep();
			
			uint32_t solid_top = args.SolidTopColor();
			uint32_t solid_bottom = args.SolidBottomColor();
			bool fadeSky = args.FadeSky();
			
			// Find bands for top solid color, top fade, center textured, bottom fade, bottom solid color:
			int start_fade = 2; // How fast it should fade out
			int fade_length = (1 << (24 - start_fade));
			int start_fadetop_y = (-frac) / fracstep;
			int end_fadetop_y = (fade_length - frac) / fracstep;
			int start_fadebottom_y = ((2 << 24) - fade_length - frac) / fracstep;
			int end_fadebottom_y = ((2 << 24) - frac) / fracstep;
			start_fadetop_y = clamp(start_fadetop_y, 0, count);
			end_fadetop_y = clamp(end_fadetop_y, 0, count);
			start_fadebottom_y = clamp(start_fadebottom_y, 0, count);
			end_fadebottom_y = clamp(end_fadebottom_y, 0, count);

			int num_cores = thread->num_cores;
			int skipped = thread->skipped_by_thread(args.DestY());
			dest = thread->dest_for_thread(args.DestY(), pitch, dest);
			frac += fracstep * skipped;
			fracstep *= num_cores;
			pitch *= num_cores;

			if (!fadeSky)
			{
				count = thread->count_for_thread(args.DestY(), count);

				for (int index = 0; index < count; index++)
				{
					uint32_t sample_index = (((((uint32_t)frac) << 8) >> FRACBITS) * textureheight0) >> FRACBITS;
					uint32_t fg = source0[sample_index];
					if (fg == 0)
					{
						uint32_t sample_index2 = MIN(sample_index, maxtextureheight1);
						fg = source1[sample_index2];
					}

					*dest = fg;
					dest += pitch;
					frac += fracstep;
				}

				return;
			}

			__m128i solid_top_fill = _mm_unpacklo_epi8(_mm_cvtsi32_si128(solid_top), _mm_setzero_si128());
			__m128i solid_bottom_fill = _mm_unpacklo_epi8(_mm_cvtsi32_si128(solid_bottom), _mm_setzero_si128());

			int index = skipped;

			// Top solid color:
			while (index < start_fadetop_y)
			{
				*dest = solid_top;
				dest += pitch;
				frac += fracstep;
				index += num_cores;
			}

			// Top fade:
			while (index < end_fadetop_y)
			{
				uint32_t sample_index = (((((uint32_t)frac) << 8) >> FRACBITS) * textureheight0) >> FRACBITS;
				uint32_t fg = source0[sample_index];
				if (fg == 0)
				{
					uint32_t sample_index2 = MIN(sample_index, maxtextureheight1);
					fg = source1[sample_index2];
				}

				__m128i alpha = _mm_set1_epi16(MAX(MIN(frac >> (16 - start_fade), 256), 0));
				__m128i inv_alpha = _mm_sub_epi16(_mm_set1_epi16(256), alpha);
				
				__m128i c = _mm_unpacklo_epi8(_mm_cvtsi32_si128(fg), _mm_setzero_si128());
				c = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(c, alpha), _mm_mullo_epi16(solid_top_fill, inv_alpha)), 8);
				*dest = _mm_cvtsi128_si32(_mm_packus_epi16(c, _mm_setzero_si128()));

				frac += fracstep;
				dest += pitch;
				index += num_cores;
			}

			// Textured center:
			while (index < start_fadebottom_y)
			{
				uint32_t sample_index = (((((uint32_t)frac) << 8) >> FRACBITS) * textureheight0) >> FRACBITS;
				uint32_t fg = source0[sample_index];
				if (fg == 0)
				{
					uint32_t sample_index2 = MIN(sample_index, maxtextureheight1);
					fg = source1[sample_index2];
				}
				*dest = fg;

				frac += fracstep;
				dest += pitch;
				index += num_cores;
			}

			// Fade bottom:
			while (index < end_fadebottom_y)
			{
				uint32_t sample_index = (((((uint32_t)frac) << 8) >> FRACBITS) * textureheight0) >> FRACBITS;
				uint32_t fg = source0[sample_index];
				if (fg == 0)
				{
					uint32_t sample_index2 = MIN(sample_index, maxtextureheight1);
					fg = source1[sample_index2];
				}

				__m128i alpha = _mm_set1_epi16(MAX(MIN(((2 << 24) - frac) >> (16 - start_fade), 256), 0));
				__m128i inv_alpha = _mm_sub_epi16(_mm_set1_epi16(256), alpha);
				
				__m128i c = _mm_unpacklo_epi8(_mm_cvtsi32_si128(fg), _mm_setzero_si128());
				c = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(c, alpha), _mm_mullo_epi16(solid_top_fill, inv_alpha)), 8);
				*dest = _mm_cvtsi128_si32(_mm_packus_epi16(c, _mm_setzero_si128()));

				frac += fracstep;
				dest += pitch;
				index += num_cores;
			}

			// Bottom solid color:
			while (index < count)
			{
				*dest = solid_bottom;
				dest += pitch;
				index += num_cores;
			}
		}
	};
}
