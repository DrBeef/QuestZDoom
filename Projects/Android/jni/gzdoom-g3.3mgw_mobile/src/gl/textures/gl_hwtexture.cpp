// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2004-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** gltexture.cpp
** Low level OpenGL texture handling. These classes are also
** containers for the various translations a texture can have.
**
*/

#include "gl/system/gl_system.h"
#include "templates.h"
#include "m_crc32.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "v_palette.h"

#include "gl/system/gl_interface.h"
#include "gl/system/gl_cvars.h"
#include "gl/system/gl_debug.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/textures/gl_material.h"


extern TexFilter_s TexFilter[];
extern int TexFormat[];


//===========================================================================
// 
//	Static texture data
//
//===========================================================================
unsigned int FHardwareTexture::lastbound[FHardwareTexture::MAX_TEXTURES];

//===========================================================================
// 
//	Quick'n dirty image rescaling.
//
// This will only be used when the source texture is larger than
// what the hardware can manage (extremely rare in Doom)
//
// Code taken from wxWidgets
//
//===========================================================================

struct BoxPrecalc
{
	int boxStart;
	int boxEnd;
};

static void ResampleBoxPrecalc(TArray<BoxPrecalc>& boxes, int oldDim)
{
	int newDim = boxes.Size();
	const double scale_factor_1 = double(oldDim) / newDim;
	const int scale_factor_2 = (int)(scale_factor_1 / 2);

	for (int dst = 0; dst < newDim; ++dst)
	{
		// Source pixel in the Y direction
		const int src_p = int(dst * scale_factor_1);

		BoxPrecalc& precalc = boxes[dst];
		precalc.boxStart = clamp<int>(int(src_p - scale_factor_1 / 2.0 + 1), 0, oldDim - 1);
		precalc.boxEnd = clamp<int>(MAX<int>(precalc.boxStart + 1, int(src_p + scale_factor_2)), 0, oldDim - 1);
	}
}

void FHardwareTexture::Resize(int width, int height, unsigned char *src_data, unsigned char *dst_data)
{

	// This function implements a simple pre-blur/box averaging method for
	// downsampling that gives reasonably smooth results To scale the image
	// down we will need to gather a grid of pixels of the size of the scale
	// factor in each direction and then do an averaging of the pixels.

	TArray<BoxPrecalc> vPrecalcs(height, true);
	TArray<BoxPrecalc> hPrecalcs(width, true);

	ResampleBoxPrecalc(vPrecalcs, texheight);
	ResampleBoxPrecalc(hPrecalcs, texwidth);

	int averaged_pixels, averaged_alpha, src_pixel_index;
	double sum_r, sum_g, sum_b, sum_a;

	for (int y = 0; y < height; y++)         // Destination image - Y direction
	{
		// Source pixel in the Y direction
		const BoxPrecalc& vPrecalc = vPrecalcs[y];

		for (int x = 0; x < width; x++)      // Destination image - X direction
		{
			// Source pixel in the X direction
			const BoxPrecalc& hPrecalc = hPrecalcs[x];

			// Box of pixels to average
			averaged_pixels = 0;
			averaged_alpha = 0;
			sum_r = sum_g = sum_b = sum_a = 0.0;

			for (int j = vPrecalc.boxStart; j <= vPrecalc.boxEnd; ++j)
			{
				for (int i = hPrecalc.boxStart; i <= hPrecalc.boxEnd; ++i)
				{
					// Calculate the actual index in our source pixels
					src_pixel_index = j * texwidth + i;

					int a = src_data[src_pixel_index * 4 + 3];
					if (a > 0)	// do not use color from fully transparent pixels
					{
						sum_r += src_data[src_pixel_index * 4 + 0];
						sum_g += src_data[src_pixel_index * 4 + 1];
						sum_b += src_data[src_pixel_index * 4 + 2];
						sum_a += a;
						averaged_pixels++;
					}
					averaged_alpha++;

				}
			}

			// Calculate the average from the sum and number of averaged pixels
			dst_data[0] = (unsigned char)xs_CRoundToInt(sum_r / averaged_pixels);
			dst_data[1] = (unsigned char)xs_CRoundToInt(sum_g / averaged_pixels);
			dst_data[2] = (unsigned char)xs_CRoundToInt(sum_b / averaged_pixels);
			dst_data[3] = (unsigned char)xs_CRoundToInt(sum_a / averaged_alpha);
			dst_data += 4;
		}
	}
}

#ifdef __MOBILE__
static void BGRAtoRGBA(unsigned char * buffer, int numPixels)
{
    uint32_t *temp = (uint32_t *)buffer;
    for( int n = 0; n < numPixels; n++ )
    {
        //temp[n] = ((temp[n] & 0x0000FF00) << 16 ) | ((temp[n] & 0xFF000000) >> 16 ) | (temp[n] & 0x00FF00FF);
        temp[n] = ((temp[n] & 0x000000FF) << 16 ) | ((temp[n] & 0x00FF0000) >> 16 ) | (temp[n] & 0xFF00FF00);
    }
}

static void GL_ResampleTexture (uint32_t *in, uint32_t inwidth, uint32_t inheight, uint32_t *out,  uint32_t outwidth, uint32_t outheight)
{
	LOGI("GL_ResampleTexture %dx%d -> %dx%d",inwidth,inheight,outwidth,outheight);

	int		i, j;
	uint32_t	*inrow, *inrow2;
	uint32_t	frac, fracstep;
	uint8_t		*pix1, *pix2, *pix3, *pix4;
	uint32_t	*p1 = (uint32_t*)malloc(sizeof(uint32_t) * outwidth );
	uint32_t	*p2 = (uint32_t*)malloc(sizeof(uint32_t) * outwidth );

	fracstep = inwidth*0x10000/outwidth;

	frac = fracstep>>2;
	for (i=0 ; i<outwidth ; i++)
	{
		p1[i] = 4*(frac>>16);
		frac += fracstep;
	}
	frac = 3*(fracstep>>2);
	for (i=0 ; i<outwidth ; i++)
	{
		p2[i] = 4*(frac>>16);
		frac += fracstep;
	}

	for (i=0 ; i<outheight ; i++, out += outwidth)
	{
		inrow = in + inwidth*(int)((i+0.25)*inheight/outheight);
		inrow2 = in + inwidth*(int)((i+0.75)*inheight/outheight);
		frac = fracstep >> 1;
		for (j=0 ; j<outwidth ; j++)
		{
			pix1 = (uint8_t *)inrow + p1[j];
			pix2 = (uint8_t *)inrow + p2[j];
			pix3 = (uint8_t *)inrow2 + p1[j];
			pix4 = (uint8_t *)inrow2 + p2[j];
			((uint8_t *)(out+j))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0])>>2;
			((uint8_t *)(out+j))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1])>>2;
			((uint8_t *)(out+j))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2])>>2;
			((uint8_t *)(out+j))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3])>>2;
		}
	}

	free(p1);
	free(p2);
}
#endif
//===========================================================================
// 
//	Loads the texture image into the hardware
//
// NOTE: For some strange reason I was unable to find the source buffer
// should be one line higher than the actual texture. I got extremely
// strange crashes deep inside the GL driver when I didn't do it!
//
//===========================================================================

#ifdef __MOBILE__
unsigned int FHardwareTexture::CreateTexture(unsigned char * buffer, int w, int h, int texunit, bool mipmap, int translation, const FString &name, bool material)
#else
unsigned int FHardwareTexture::CreateTexture(unsigned char * buffer, int w, int h, int texunit, bool mipmap, int translation, const FString &name)
#endif
{
	int rh,rw;
	int texformat=TexFormat[gl_texture_format];
	bool deletebuffer=false;

	if (forcenocompression)
	{
		texformat = GL_RGBA8;
	}
	TranslatedTexture * glTex=GetTexID(translation);
	if (glTex->glTexID==0) glGenTextures(1,&glTex->glTexID);
	if (texunit != 0) glActiveTexture(GL_TEXTURE0+texunit);
	glBindTexture(GL_TEXTURE_2D, glTex->glTexID);
	FGLDebug::LabelObject(GL_TEXTURE, glTex->glTexID, name);
	lastbound[texunit] = glTex->glTexID;

	if (!buffer)
	{
		w=texwidth;
		h=abs(texheight);
		rw = GetTexDimension (w);
		rh = GetTexDimension (h);

		// The texture must at least be initialized if no data is present.
		glTex->mipmapped = false;
		buffer=(unsigned char *)calloc(4,rw * (rh+1));
		deletebuffer=true;
		//texheight=-h;	
	}
	else
	{
#ifdef __MOBILE__
        if( gl.glesVer == 1 )
        {
    	    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, (mipmap && TexFilter[gl_texture_filter].mipmapping) );
    	}
#endif
		rw = GetTexDimension (w);
		rh = GetTexDimension (h);

#ifdef __MOBILE__
       if (rw == w && rh == h) // Same size, do nothing
		{
		}
		else if (rw < w || rh < h)
		{
			// The texture is larger than what the hardware can handle so scale it down.
			unsigned char * scaledbuffer=(unsigned char *)calloc(4,rw * (rh+1));
			if (scaledbuffer)
			{
				Resize(rw, rh, buffer, scaledbuffer);
				deletebuffer=true;
				buffer=scaledbuffer;
			}
		}
		// Need to find a way to work out if 3d texture
		else if (material) // Need to make bigger by resampling, for 3d textures
        {
            unsigned int * scaledbuffer=(unsigned int *)calloc(4,rw * (rh+1));
            GL_ResampleTexture((unsigned  *)buffer,w,h,(unsigned *)scaledbuffer,rw,rh);
            deletebuffer=true;
            buffer=(unsigned char *)scaledbuffer;
        }

        else // Need to put into bigger texture, but DONT resample, for 2d images. Works because coordinates are scaled when rendering
        {
            unsigned int * scaledbuffer=(unsigned int *)calloc(4,rw * (rh+1));
            for(int y = 0; y < h; y++)
                for( int x = 0; x < w; x++ )
                {
                    scaledbuffer[x + y * rw] = ((unsigned int *)buffer)[x + y * w];
                }
            deletebuffer=true;
            buffer=(unsigned char *)scaledbuffer;
        }
#else
		if (rw < w || rh < h)
		{
			// The texture is larger than what the hardware can handle so scale it down.
			unsigned char * scaledbuffer=(unsigned char *)calloc(4,rw * (rh+1));
			if (scaledbuffer)
			{
				Resize(rw, rh, buffer, scaledbuffer);
				deletebuffer=true;
				buffer=scaledbuffer;
			}
		}
#endif
	}

#ifdef __MOBILE__
    
    if(!(gl.flags & RFL_BGRA))
    {
        texformat = GL_RGBA;
        BGRAtoRGBA( buffer, rw * rh );
    }
    else
    {
        texformat = GL_BGRA;
    }
    glTexImage2D(GL_TEXTURE_2D, 0, texformat, rw, rh, 0, texformat, GL_UNSIGNED_BYTE, buffer);
#else
	glTexImage2D(GL_TEXTURE_2D, 0, texformat, rw, rh, 0, GL_BGRA, GL_UNSIGNED_BYTE, buffer);
#endif
	if (deletebuffer) free(buffer);

#ifdef __MOBILE__
    if( gl.glesVer > 1 )
#endif
	if (mipmap && TexFilter[gl_texture_filter].mipmapping)
	{
		glGenerateMipmap(GL_TEXTURE_2D);
		glTex->mipmapped = true;
	}
#ifdef __MOBILE__
    if( gl.glesVer == 1 )
    {
    	bool use_mipmapping = TexFilter[gl_texture_filter].mipmapping;
	
	    if (mipmap && use_mipmapping)
	    {
	        glTex->mipmapped = true;
	    }
	    else
	    {
	        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, TexFilter[gl_texture_filter].magfilter);
	    }
	    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, TexFilter[gl_texture_filter].magfilter);
	}
#endif

	if (texunit != 0) glActiveTexture(GL_TEXTURE0);
	return glTex->glTexID;
}


//===========================================================================
// 
//	Creates a texture
//
//===========================================================================
FHardwareTexture::FHardwareTexture(int _width, int _height, bool nocompression) 
{
	forcenocompression = nocompression;
	texwidth=_width;
	texheight=_height;

	glDefTex.glTexID = 0;
	glDefTex.translation = 0;
	glDefTex.mipmapped = false;
	glDepthID = 0;
}


//===========================================================================
// 
//	Deletes a texture id and unbinds it from the texture units
//
//===========================================================================
void FHardwareTexture::TranslatedTexture::Delete()
{
	if (glTexID != 0) 
	{
		for(int i = 0; i < MAX_TEXTURES; i++)
		{
			if (lastbound[i] == glTexID)
			{
				lastbound[i] = 0;
			}
		}
		glDeleteTextures(1, &glTexID);
		glTexID = 0;
		mipmapped = false;
	}
}

//===========================================================================
// 
//	Frees all associated resources
//
//===========================================================================
void FHardwareTexture::Clean(bool all)
{
	int cm_arraysize = CM_FIRSTSPECIALCOLORMAP + SpecialColormaps.Size();

	if (all)
	{
		glDefTex.Delete();
	}
	for(unsigned int i=0;i<glTex_Translated.Size();i++)
	{
		glTex_Translated[i].Delete();
	}
	glTex_Translated.Clear();
	if (glDepthID != 0) glDeleteRenderbuffers(1, &glDepthID);
	glDepthID = 0;
}

//===========================================================================
// 
// Deletes all allocated resources and considers translations
// This will only be called for sprites
//
//===========================================================================

void FHardwareTexture::CleanUnused(SpriteHits &usedtranslations)
{
	if (usedtranslations.CheckKey(0) == nullptr)
	{
		glDefTex.Delete();
	}
	for (int i = glTex_Translated.Size()-1; i>= 0; i--)
	{
		if (usedtranslations.CheckKey(glTex_Translated[i].translation) == nullptr)
		{
			glTex_Translated[i].Delete();
			glTex_Translated.Delete(i);
		}
	}
}

//===========================================================================
// 
//	Destroys the texture
//
//===========================================================================
FHardwareTexture::~FHardwareTexture() 
{ 
	Clean(true); 
}


//===========================================================================
// 
//	Gets a texture ID address and validates all required data
//
//===========================================================================

FHardwareTexture::TranslatedTexture *FHardwareTexture::GetTexID(int translation)
{
	if (translation == 0)
	{
		return &glDefTex;
	}

	// normally there aren't more than very few different 
	// translations here so this isn't performance critical.
	for (unsigned int i = 0; i < glTex_Translated.Size(); i++)
	{
		if (glTex_Translated[i].translation == translation)
		{
			return &glTex_Translated[i];
		}
	}

	int add = glTex_Translated.Reserve(1);
	glTex_Translated[add].translation = translation;
	glTex_Translated[add].glTexID = 0;
	glTex_Translated[add].mipmapped = false;
	return &glTex_Translated[add];
}

//===========================================================================
// 
//	Binds this patch
//
//===========================================================================
unsigned int FHardwareTexture::Bind(int texunit, int translation, bool needmipmap)
{
	TranslatedTexture *pTex = GetTexID(translation);

	if (pTex->glTexID != 0)
	{
		if (lastbound[texunit] == pTex->glTexID) return pTex->glTexID;
		lastbound[texunit] = pTex->glTexID;
		if (texunit != 0) glActiveTexture(GL_TEXTURE0 + texunit);
		glBindTexture(GL_TEXTURE_2D, pTex->glTexID);
		// Check if we need mipmaps on a texture that was creted without them.
		if (needmipmap && !pTex->mipmapped && TexFilter[gl_texture_filter].mipmapping)
		{
			glGenerateMipmap(GL_TEXTURE_2D);
			pTex->mipmapped = true;
		}
		if (texunit != 0) glActiveTexture(GL_TEXTURE0);
		return pTex->glTexID;
	}
	return 0;
}

unsigned int FHardwareTexture::GetTextureHandle(int translation)
{
	TranslatedTexture *pTex = GetTexID(translation);
	return pTex->glTexID;
}

void FHardwareTexture::Unbind(int texunit)
{
	if (lastbound[texunit] != 0)
	{
		if (texunit != 0) glActiveTexture(GL_TEXTURE0+texunit);
		glBindTexture(GL_TEXTURE_2D, 0);
		if (texunit != 0) glActiveTexture(GL_TEXTURE0);
		lastbound[texunit] = 0;
	}
}

void FHardwareTexture::UnbindAll()
{
	for(int texunit = 0; texunit < 16; texunit++)
	{
		Unbind(texunit);
	}
	FMaterial::ClearLastTexture();
}

//===========================================================================
// 
//	Creates a depth buffer for this texture
//
//===========================================================================

int FHardwareTexture::GetDepthBuffer()
{
	if (glDepthID == 0)
	{
		glGenRenderbuffers(1, &glDepthID);
		glBindRenderbuffer(GL_RENDERBUFFER, glDepthID);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 
			GetTexDimension(texwidth), GetTexDimension(texheight));
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
	}
	return glDepthID;
}


//===========================================================================
// 
//	Binds this texture's surfaces to the current framrbuffer
//
//===========================================================================

void FHardwareTexture::BindToFrameBuffer()
{
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glDefTex.glTexID, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, GetDepthBuffer());
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, GetDepthBuffer());
}

