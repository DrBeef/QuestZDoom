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
** gl_shader.cpp
**
** GLSL shader handling
**
*/

#include "gl/system/gl_system.h"
#include "c_cvars.h"
#include "v_video.h"
#include "name.h"
#include "w_wad.h"
#include "i_system.h"
#include "doomerrors.h"
#include "v_palette.h"
#include "sc_man.h"
#include "cmdlib.h"
#include "vm.h"
#include "d_player.h"
#include "md5.h"
#include "m_misc.h"

#include "gl/system/gl_interface.h"
#include "gl/system/gl_debug.h"
#include "gl/data/gl_data.h"
#include "r_data/matrix.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/system/gl_cvars.h"
#include "gl/shaders/gl_shader.h"
#include "gl/shaders/gl_shaderprogram.h"
#include "gl/shaders/gl_postprocessshader.h"
#include "gl/textures/gl_material.h"
#include "gl/dynlights/gl_lightbuffer.h"
#include <map>
#include <memory>


#ifdef __MOBILE__
CVAR(Bool, gl_customshader, true, 0)
#endif

static bool IsGlslWhitespace(char c)
{
	switch (c)
	{
	case ' ':
	case '\r':
	case '\n':
	case '\t':
	case '\f':
		return true;
	default:
		return false;
	}
}

static FString NextGlslToken(const char *chars, long len, long &pos)
{
	// Eat whitespace
	long tokenStart = pos;
	while (tokenStart != len && IsGlslWhitespace(chars[tokenStart]))
		tokenStart++;

	// Find token end
	long tokenEnd = tokenStart;
	while (tokenEnd != len && !IsGlslWhitespace(chars[tokenEnd]) && chars[tokenEnd] != ';')
		tokenEnd++;

	pos = tokenEnd;
	return FString(chars + tokenStart, tokenEnd - tokenStart);
}

static FString RemoveLegacyUserUniforms(FString code)
{
	// User shaders must declare their uniforms via the GLDEFS file.
	// The following code searches for legacy uniform declarations in the shader itself and replaces them with whitespace.

	long len = (long)code.Len();
	char *chars = code.LockBuffer();

	long startIndex = 0;
	while (true)
	{
		long matchIndex = code.IndexOf("uniform", startIndex);
		if (matchIndex == -1)
			break;

		bool isLegacyUniformName = false;

		bool isKeywordStart = matchIndex == 0 || IsGlslWhitespace(chars[matchIndex - 1]);
		bool isKeywordEnd = matchIndex + 7 == len || IsGlslWhitespace(chars[matchIndex + 7]);
		if (isKeywordStart && isKeywordEnd)
		{
			long pos = matchIndex + 7;
			FString type = NextGlslToken(chars, len, pos);
			FString identifier = NextGlslToken(chars, len, pos);

			isLegacyUniformName = type.Compare("float") == 0 && identifier.Compare("timer") == 0;
		}

		if (isLegacyUniformName)
		{
			long statementEndIndex = code.IndexOf(';', matchIndex + 7);
			if (statementEndIndex == -1)
				statementEndIndex = len;
			for (long i = matchIndex; i <= statementEndIndex; i++)
			{
				if (!IsGlslWhitespace(chars[i]))
					chars[i] = ' ';
			}
			startIndex = statementEndIndex;
		}
		else
		{
			startIndex = matchIndex + 7;
		}
	}

	code.UnlockBuffer();

	return code;
}

struct ProgramBinary
{
	uint32_t format;
	TArray<uint8_t> data;
};

static const char *ShaderMagic = "ZDSC";

static std::map<FString, std::unique_ptr<ProgramBinary>> ShaderCache; // Not a TMap because it doesn't support unique_ptr move semantics

bool IsShaderCacheActive()
{
	static bool active = true;
	static bool firstcall = true;

	if (firstcall)
	{
		const char *vendor = (const char *)glGetString(GL_VENDOR);
		active = strstr(vendor, "Intel") != nullptr;
		firstcall = false;
	}
	return active;
}

static FString CalcProgramBinaryChecksum(const FString &vertex, const FString &fragment)
{
	const GLubyte *vendor = glGetString(GL_VENDOR);
	const GLubyte *renderer = glGetString(GL_RENDERER);
	const GLubyte *version = glGetString(GL_VERSION);

	uint8_t digest[16];
	MD5Context md5;
	md5.Update(vendor, (unsigned int)strlen((const char*)vendor));
	md5.Update(renderer, (unsigned int)strlen((const char*)renderer));
	md5.Update(version, (unsigned int)strlen((const char*)version));
	md5.Update((const uint8_t *)vertex.GetChars(), (unsigned int)vertex.Len());
	md5.Update((const uint8_t *)fragment.GetChars(), (unsigned int)fragment.Len());
	md5.Final(digest);

	char hexdigest[33];
	for (int i = 0; i < 16; i++)
	{
		int v = digest[i] >> 4;
		hexdigest[i * 2] = v < 10 ? ('0' + v) : ('a' + v - 10);
		v = digest[i] & 15;
		hexdigest[i * 2 + 1] = v < 10 ? ('0' + v) : ('a' + v - 10);
	}
	hexdigest[32] = 0;
	return hexdigest;
}

static FString CreateProgramCacheName(bool create)
{
	FString path = M_GetCachePath(create);
	if (create) CreatePath(path);
	path << "/shadercache.zdsc";
	return path;
}

static void LoadShaders()
{
	static bool loaded = false;
	if (loaded)
		return;
	loaded = true;

	try
	{
		FString path = CreateProgramCacheName(false);
		FileReader fr;
		if (!fr.OpenFile(path))
			throw std::runtime_error("Could not open shader file");

		char magic[4];
		fr.Read(magic, 4);
		if (memcmp(magic, ShaderMagic, 4) != 0)
			throw std::runtime_error("Not a shader cache file");

		uint32_t count = fr.ReadUInt32();
		if (count > 512)
			throw std::runtime_error("Too many shaders cached");

		for (uint32_t i = 0; i < count; i++)
		{
			char hexdigest[33];
			if (fr.Read(hexdigest, 32) != 32)
				throw std::runtime_error("Read error");
			hexdigest[32] = 0;

			std::unique_ptr<ProgramBinary> binary(new ProgramBinary());
			binary->format = fr.ReadUInt32();
			uint32_t size = fr.ReadUInt32();
			if (size > 1024 * 1024)
				throw std::runtime_error("Shader too big, probably file corruption");

			binary->data.Resize(size);
			if (fr.Read(binary->data.Data(), binary->data.Size()) != binary->data.Size())
				throw std::runtime_error("Read error");

			ShaderCache[hexdigest] = std::move(binary);
		}
	}
	catch (...)
	{
		ShaderCache.clear();
	}
}

static void SaveShaders()
{
	FString path = CreateProgramCacheName(true);
	std::unique_ptr<FileWriter> fw(FileWriter::Open(path));
	if (fw)
	{
		uint32_t count = (uint32_t)ShaderCache.size();
		fw->Write(ShaderMagic, 4);
		fw->Write(&count, sizeof(uint32_t));
		for (const auto &it : ShaderCache)
		{
			uint32_t size = it.second->data.Size();
			fw->Write(it.first.GetChars(), 32);
			fw->Write(&it.second->format, sizeof(uint32_t));
			fw->Write(&size, sizeof(uint32_t));
			fw->Write(it.second->data.Data(), it.second->data.Size());
		}
	}
}

TArray<uint8_t> LoadCachedProgramBinary(const FString &vertex, const FString &fragment, uint32_t &binaryFormat)
{
	LoadShaders();

	auto it = ShaderCache.find(CalcProgramBinaryChecksum(vertex, fragment));
	if (it != ShaderCache.end())
	{
		binaryFormat = it->second->format;
		return it->second->data;
	}
	else
	{
		binaryFormat = 0;
		return {};
	}
}

void SaveCachedProgramBinary(const FString &vertex, const FString &fragment, const TArray<uint8_t> &binary, uint32_t binaryFormat)
{
	auto &entry = ShaderCache[CalcProgramBinaryChecksum(vertex, fragment)];
	entry.reset(new ProgramBinary());
	entry->format = binaryFormat;
	entry->data = binary;

	SaveShaders();
}

bool FShader::Load(const char * name, const char * vert_prog_lump, const char * frag_prog_lump, const char * proc_prog_lump, const char * light_fragprog, const char * defines)
{
	static char buffer[10000];
	FString error;

	FString i_data;
	
	// these settings are actually pointless but there seem to be some old ATI drivers that fail to compile the shader without setting the precision here.
	i_data += "precision highp int;\n";
	i_data += "precision highp float;\n";
#ifdef __MOBILE__
    i_data += "precision highp sampler2D;\n";
    i_data += "precision highp samplerCube;\n";
    i_data += "precision highp sampler2DMS;\n";
#endif
	i_data += "uniform vec4 uCameraPos;\n";
	i_data += "uniform int uTextureMode;\n";
	i_data += "uniform float uClipHeight;\n";
	i_data += "uniform float uClipHeightDirection;\n";
	i_data += "uniform int uShadowmapFilter;\n";
	i_data += "uniform vec2 uClipSplit;\n";
	i_data += "uniform vec4 uClipLine;\n";
	i_data += "uniform float uAlphaThreshold;\n";

	// colors
	i_data += "uniform vec4 uObjectColor;\n";
	i_data += "uniform vec4 uObjectColor2;\n";
	i_data += "uniform vec4 uDynLightColor;\n";
	i_data += "uniform vec4 uAddColor;\n";
	i_data += "uniform vec4 uFogColor;\n";
	i_data += "uniform float uDesaturationFactor;\n";
	i_data += "uniform float uInterpolationFactor;\n";

	// Fixed colormap stuff
	i_data += "uniform int uFixedColormap;\n"; // 0, when no fixed colormap, 1 for a light value, 2 for a color blend, 3 for a fog layer
	i_data += "uniform vec4 uFixedColormapStart;\n";
	i_data += "uniform vec4 uFixedColormapRange;\n";

	// Glowing walls stuff
	i_data += "uniform vec4 uGlowTopPlane;\n";
	i_data += "uniform vec4 uGlowTopColor;\n";
	i_data += "uniform vec4 uGlowBottomPlane;\n";
	i_data += "uniform vec4 uGlowBottomColor;\n";

	i_data += "uniform vec4 uGradientTopPlane;\n";
	i_data += "uniform vec4 uGradientBottomPlane;\n";

	i_data += "uniform vec4 uSplitTopPlane;\n";
	i_data += "uniform vec4 uSplitBottomPlane;\n";

	// Lighting + Fog
	i_data += "uniform vec4 uLightAttr;\n";
	i_data += "#define uLightLevel uLightAttr.a\n";
	i_data += "#define uFogDensity uLightAttr.b\n";
	i_data += "#define uLightFactor uLightAttr.g\n";
	i_data += "#define uLightDist uLightAttr.r\n";
	i_data += "uniform int uFogEnabled;\n";
	i_data += "uniform int uPalLightLevels;\n";
	i_data += "uniform float uGlobVis;\n"; // uGlobVis = R_GetGlobVis(r_visibility) / 32.0

	// dynamic lights
	i_data += "uniform int uLightIndex;\n";

	// Software fuzz scaling
	i_data += "uniform int uViewHeight;\n";

	// Blinn glossiness and specular level
	i_data += "uniform vec2 uSpecularMaterial;\n";

	// quad drawer stuff
	i_data += "#ifdef USE_QUAD_DRAWER\n";
	i_data += "uniform mat4 uQuadVertices;\n";
	i_data += "uniform mat4 uQuadTexCoords;\n";
	i_data += "uniform int uQuadMode;\n";
	i_data += "#endif\n";

	// matrices
	i_data += "uniform mat4 ProjectionMatrix;\n";
	i_data += "uniform mat4 ViewMatrix;\n";
	i_data += "uniform mat4 ModelMatrix;\n";
	i_data += "uniform mat4 NormalViewMatrix;\n";
	i_data += "uniform mat4 NormalModelMatrix;\n";
	i_data += "uniform mat4 TextureMatrix;\n";

	// light buffers
	i_data += "#ifdef SHADER_STORAGE_LIGHTS\n";
	i_data += "layout(std430, binding = 1) buffer LightBufferSSO\n";
	i_data += "{\n";
	i_data += "    vec4 lights[];\n";
	i_data += "};\n";
	i_data += "#elif defined NUM_UBO_LIGHTS\n";
	i_data += "uniform LightBufferUBO\n";
	i_data += "{\n";
	i_data += "    vec4 lights[NUM_UBO_LIGHTS];\n";
	i_data += "};\n";
	i_data += "#endif\n";

	// textures
	i_data += "uniform sampler2D tex;\n";
	i_data += "uniform sampler2D ShadowMap;\n";
	i_data += "uniform sampler2D texture2;\n";
	i_data += "uniform sampler2D texture3;\n";
	i_data += "uniform sampler2D texture4;\n";
	i_data += "uniform sampler2D texture5;\n";
	i_data += "uniform sampler2D texture6;\n";

	// timer data
	i_data += "uniform float timer;\n"; // To do: we must search user shaders for this declaration and remove it

	// material types
	i_data += "#if defined(SPECULAR)\n";
	i_data += "#define normaltexture texture2\n";
	i_data += "#define speculartexture texture3\n";
	i_data += "#define brighttexture texture4\n";
	i_data += "#elif defined(PBR)\n";
	i_data += "#define normaltexture texture2\n";
	i_data += "#define metallictexture texture3\n";
	i_data += "#define roughnesstexture texture4\n";
	i_data += "#define aotexture texture5\n";
	i_data += "#define brighttexture texture6\n";
	i_data += "#else\n";
	i_data += "#define brighttexture texture2\n";
	i_data += "#endif\n";

	int vp_lump = Wads.CheckNumForFullName(vert_prog_lump, 0);
	if (vp_lump == -1) I_Error("Unable to load '%s'", vert_prog_lump);
	FMemLump vp_data = Wads.ReadLump(vp_lump);

	int fp_lump = Wads.CheckNumForFullName(frag_prog_lump, 0);
	if (fp_lump == -1) I_Error("Unable to load '%s'", frag_prog_lump);
	FMemLump fp_data = Wads.ReadLump(fp_lump);



//
// The following code uses GetChars on the strings to get rid of terminating 0 characters. Do not remove or the code may break!
//
	FString vp_comb;

	assert(GLRenderer->mLights != NULL);
	// On the shader side there is no difference between LM_DEFERRED and LM_DIRECT, it only decides how the buffer is initialized.
	unsigned int lightbuffertype = GLRenderer->mLights->GetBufferType();
	unsigned int lightbuffersize = GLRenderer->mLights->GetBlockSize();
#ifdef __MOBILE__
	if (gl_light_buffer_type == 0)
		vp_comb.Format(ES_VERSION_STR"\n#define NUM_UBO_LIGHTS %d\n", lightbuffersize);
	else
		vp_comb.Format(ES_VERSION_STR"\n#define SHADER_STORAGE_LIGHTS\n");
#else
	if (lightbuffertype == GL_UNIFORM_BUFFER)
	{
		if (gl.es)
		{
			vp_comb.Format("#version 300 es\n#define NUM_UBO_LIGHTS %d\n", lightbuffersize);
		}
		else if (gl.glslversion < 1.4f) // This differentiation is for some Intel drivers which fail on #extension, so use of #version 140 is necessary
		{
			vp_comb.Format("#version 130\n#extension GL_ARB_uniform_buffer_object : require\n#define NUM_UBO_LIGHTS %d\n", lightbuffersize);
		}
		else
		{
			vp_comb.Format("#version 140\n#define NUM_UBO_LIGHTS %d\n", lightbuffersize);
		}
	}
	else
	{
		// This differentiation is for Intel which do not seem to expose the full extension, even if marked as required.
		if (gl.glslversion < 4.3f)
			vp_comb = "#version 400 core\n#extension GL_ARB_shader_storage_buffer_object : require\n#define SHADER_STORAGE_LIGHTS\n";
		else
			vp_comb = "#version 430 core\n#define SHADER_STORAGE_LIGHTS\n";
	}
#endif

	if (gl.buffermethod == BM_DEFERRED)
	{
		vp_comb << "#define USE_QUAD_DRAWER\n";
	}

	if (!!(gl.flags & RFL_SHADER_STORAGE_BUFFER))
	{
		vp_comb << "#define SUPPORTS_SHADOWMAPS\n";
	}

	vp_comb << defines << i_data.GetChars();
	FString fp_comb = vp_comb;

	vp_comb << "#line 1\n";
	fp_comb << "#line 1\n";

	vp_comb << vp_data.GetString().GetChars() << "\n";
	fp_comb << fp_data.GetString().GetChars() << "\n";

	if (proc_prog_lump != NULL)
	{
		fp_comb << "#line 1\n";

		if (*proc_prog_lump != '#')
		{
			int pp_lump = Wads.CheckNumForFullName(proc_prog_lump);
			if (pp_lump == -1) I_Error("Unable to load '%s'", proc_prog_lump);
			FMemLump pp_data = Wads.ReadLump(pp_lump);

			if (pp_data.GetString().IndexOf("ProcessMaterial") < 0)
			{
				// this looks like an old custom hardware shader.

				// add ProcessMaterial function that calls the older ProcessTexel function
				int pl_lump = Wads.CheckNumForFullName("shaders/glsl/func_defaultmat.fp");
				if (pl_lump == -1) I_Error("Unable to load '%s'", "shaders/glsl/func_defaultmat.fp");
				FMemLump pl_data = Wads.ReadLump(pl_lump);
				fp_comb << "\n" << pl_data.GetString().GetChars();

				if (pp_data.GetString().IndexOf("ProcessTexel") < 0)
				{
					// this looks like an even older custom hardware shader.
					// We need to replace the ProcessTexel call to make it work.

					fp_comb.Substitute("material.Base = ProcessTexel();", "material.Base = Process(vec4(1.0));");
				}

				if (pp_data.GetString().IndexOf("ProcessLight") >= 0)
				{
					// The ProcessLight signatured changed. Forward to the old one.
					fp_comb << "\nvec4 ProcessLight(vec4 color);\n";
					fp_comb << "\nvec4 ProcessLight(Material material, vec4 color) { return ProcessLight(color); }\n";
				}
			}

			fp_comb << RemoveLegacyUserUniforms(pp_data.GetString()).GetChars();
			fp_comb.Substitute("gl_TexCoord[0]", "vTexCoord");	// fix old custom shaders.

			if (pp_data.GetString().IndexOf("ProcessLight") < 0)
			{
				int pl_lump = Wads.CheckNumForFullName("shaders/glsl/func_defaultlight.fp");
				if (pl_lump == -1) I_Error("Unable to load '%s'", "shaders/glsl/func_defaultlight.fp");
				FMemLump pl_data = Wads.ReadLump(pl_lump);
				fp_comb << "\n" << pl_data.GetString().GetChars();
			}
		}
		else
		{
			// Proc_prog_lump is not a lump name but the source itself (from generated shaders)
			fp_comb << proc_prog_lump + 1;
		}
	}

	if (light_fragprog)
	{
		int pp_lump = Wads.CheckNumForFullName(light_fragprog);
		if (pp_lump == -1) I_Error("Unable to load '%s'", light_fragprog);
		FMemLump pp_data = Wads.ReadLump(pp_lump);
		fp_comb << pp_data.GetString().GetChars() << "\n";
	}

	if (gl.flags & RFL_NO_CLIP_PLANES)
	{
		// On ATI's GL3 drivers we have to disable gl_ClipDistance because it's hopelessly broken.
		// This will cause some glitches and regressions but is the only way to avoid total display garbage.
		vp_comb.Substitute("gl_ClipDistance", "//");
	}

	hShader = glCreateProgram();
	FGLDebug::LabelObject(GL_PROGRAM, hShader, name);

	uint32_t binaryFormat = 0;
	TArray<uint8_t> binary;
	if (IsShaderCacheActive())
		binary = LoadCachedProgramBinary(vp_comb, fp_comb, binaryFormat);

	bool linked = false;
	if (binary.Size() > 0 && glProgramBinary)
	{
		glProgramBinary(hShader, binaryFormat, binary.Data(), binary.Size());
		GLint status = 0;
		glGetProgramiv(hShader, GL_LINK_STATUS, &status);
		linked = (status == GL_TRUE);
	}

	if (!linked)
	{
		hVertProg = glCreateShader(GL_VERTEX_SHADER);
		hFragProg = glCreateShader(GL_FRAGMENT_SHADER);

		FGLDebug::LabelObject(GL_SHADER, hVertProg, vert_prog_lump);
		FGLDebug::LabelObject(GL_SHADER, hFragProg, frag_prog_lump);

		int vp_size = (int)vp_comb.Len();
		int fp_size = (int)fp_comb.Len();

		const char *vp_ptr = vp_comb.GetChars();
		const char *fp_ptr = fp_comb.GetChars();

		glShaderSource(hVertProg, 1, &vp_ptr, &vp_size);
		glShaderSource(hFragProg, 1, &fp_ptr, &fp_size);

		glCompileShader(hVertProg);
		glCompileShader(hFragProg);

		glAttachShader(hShader, hVertProg);
		glAttachShader(hShader, hFragProg);

		glBindAttribLocation(hShader, VATTR_VERTEX, "aPosition");
		glBindAttribLocation(hShader, VATTR_TEXCOORD, "aTexCoord");
		glBindAttribLocation(hShader, VATTR_COLOR, "aColor");
		glBindAttribLocation(hShader, VATTR_VERTEX2, "aVertex2");
		glBindAttribLocation(hShader, VATTR_NORMAL, "aNormal");
#ifndef __MOBILE__
		glBindFragDataLocation(hShader, 0, "FragColor");
		glBindFragDataLocation(hShader, 1, "FragFog");
		glBindFragDataLocation(hShader, 2, "FragNormal");
#endif
		glLinkProgram(hShader);

		glGetShaderInfoLog(hVertProg, 10000, NULL, buffer);
		if (*buffer)
		{
			error << "Vertex shader:\n" << buffer << "\n";
		}
		glGetShaderInfoLog(hFragProg, 10000, NULL, buffer);
		if (*buffer)
		{
			error << "Fragment shader:\n" << buffer << "\n";
		}

		glGetProgramInfoLog(hShader, 10000, NULL, buffer);
		if (*buffer)
		{
			error << "Linking:\n" << buffer << "\n";
		}
		GLint status = 0;
		glGetProgramiv(hShader, GL_LINK_STATUS, &status);
		linked = (status == GL_TRUE);
		if (!linked)
		{
			// only print message if there's an error.
			I_Error("Init Shader '%s':\n%s\n", name, error.GetChars());
		}
		else if (glProgramBinary && IsShaderCacheActive())
		{
			int binaryLength = 0;
			glGetProgramiv(hShader, GL_PROGRAM_BINARY_LENGTH, &binaryLength);
			binary.Resize(binaryLength);
			glGetProgramBinary(hShader, binary.Size(), &binaryLength, &binaryFormat, binary.Data());
			binary.Resize(binaryLength);
			SaveCachedProgramBinary(vp_comb, fp_comb, binary, binaryFormat);
		}
	}
	else
	{
		hVertProg = 0;
		hFragProg = 0;
	}

	muDesaturation.Init(hShader, "uDesaturationFactor");
	muFogEnabled.Init(hShader, "uFogEnabled");
	muPalLightLevels.Init(hShader, "uPalLightLevels");
	muGlobVis.Init(hShader, "uGlobVis");
	muTextureMode.Init(hShader, "uTextureMode");
	muCameraPos.Init(hShader, "uCameraPos");
	muLightParms.Init(hShader, "uLightAttr");
	muClipSplit.Init(hShader, "uClipSplit");
	muColormapStart.Init(hShader, "uFixedColormapStart");
	muColormapRange.Init(hShader, "uFixedColormapRange");
	muLightIndex.Init(hShader, "uLightIndex");
	muFogColor.Init(hShader, "uFogColor");
	muDynLightColor.Init(hShader, "uDynLightColor");
	muObjectColor.Init(hShader, "uObjectColor");
	muObjectColor2.Init(hShader, "uObjectColor2");
	muGlowBottomColor.Init(hShader, "uGlowBottomColor");
	muGlowTopColor.Init(hShader, "uGlowTopColor");
	muGlowBottomPlane.Init(hShader, "uGlowBottomPlane");
	muGlowTopPlane.Init(hShader, "uGlowTopPlane");
	muGradientBottomPlane.Init(hShader, "uGradientBottomPlane");
	muGradientTopPlane.Init(hShader, "uGradientTopPlane");
	muSplitBottomPlane.Init(hShader, "uSplitBottomPlane");
	muSplitTopPlane.Init(hShader, "uSplitTopPlane");
	muClipLine.Init(hShader, "uClipLine");
	muFixedColormap.Init(hShader, "uFixedColormap");
	muInterpolationFactor.Init(hShader, "uInterpolationFactor");
	muClipHeight.Init(hShader, "uClipHeight");
	muClipHeightDirection.Init(hShader, "uClipHeightDirection");
	muShadowmapFilter.Init(hShader, "uShadowmapFilter");
	muAlphaThreshold.Init(hShader, "uAlphaThreshold");
	muSpecularMaterial.Init(hShader, "uSpecularMaterial");
	muAddColor.Init(hShader, "uAddColor");
	muViewHeight.Init(hShader, "uViewHeight");
	muTimer.Init(hShader, "timer");

	lights_index = glGetUniformLocation(hShader, "lights");
	fakevb_index = glGetUniformLocation(hShader, "fakeVB");
	projectionmatrix_index = glGetUniformLocation(hShader, "ProjectionMatrix");
	viewmatrix_index = glGetUniformLocation(hShader, "ViewMatrix");
	modelmatrix_index = glGetUniformLocation(hShader, "ModelMatrix");
	texturematrix_index = glGetUniformLocation(hShader, "TextureMatrix");
	vertexmatrix_index = glGetUniformLocation(hShader, "uQuadVertices");
	texcoordmatrix_index = glGetUniformLocation(hShader, "uQuadTexCoords");
	normalviewmatrix_index = glGetUniformLocation(hShader, "NormalViewMatrix");
	normalmodelmatrix_index = glGetUniformLocation(hShader, "NormalModelMatrix");
	quadmode_index = glGetUniformLocation(hShader, "uQuadMode");

	if (!gl.legacyMode && lightbuffertype == GL_UNIFORM_BUFFER)
	{
		int tempindex = glGetUniformBlockIndex(hShader, "LightBufferUBO");
		if (tempindex != -1) glUniformBlockBinding(hShader, tempindex, LIGHTBUF_BINDINGPOINT);
	}

	glUseProgram(hShader);
	if (quadmode_index > 0) glUniform1i(quadmode_index, 0);

	// set up other texture units (if needed by the shader)
	for (int i = 2; i<16; i++)
	{
		char stringbuf[20];
		mysnprintf(stringbuf, 20, "texture%d", i);
		int tempindex = glGetUniformLocation(hShader, stringbuf);
#ifdef __ANDROID__
		if (tempindex >= 0) glUniform1i(tempindex, i - 1);
#else
		if (tempindex > 0) glUniform1i(tempindex, i - 1);
#endif
	}

	int shadowmapindex = glGetUniformLocation(hShader, "ShadowMap");
#ifdef __ANDROID__
	if (shadowmapindex >= 0) glUniform1i(shadowmapindex, 16);
#else
		if (shadowmapindex > 0) glUniform1i(shadowmapindex, 16);
#endif
	glUseProgram(0);
	return linked;
}

//==========================================================================
//
//
//
//==========================================================================

FShader::~FShader()
{
	glDeleteProgram(hShader);
	if (hVertProg != 0)
		glDeleteShader(hVertProg);
	if (hFragProg != 0)
		glDeleteShader(hFragProg);
}


//==========================================================================
//
//
//
//==========================================================================

bool FShader::Bind()
{
	GLRenderer->mShaderManager->SetActiveShader(this);
	return true;
}

//==========================================================================
//
// Since all shaders are REQUIRED, any error here needs to be fatal
//
//==========================================================================

FShader *FShaderCollection::Compile (const char *ShaderName, const char *ShaderPath, const char *LightModePath, const char *shaderdefines, bool usediscard, EPassType passType)
{
	FString defines;
	defines += shaderdefines;
	// this can't be in the shader code due to ATI strangeness.
	if (gl.MaxLights() == 128) defines += "#define MAXLIGHTS128\n";
	if (!usediscard) defines += "#define NO_ALPHATEST\n";
	if (passType == GBUFFER_PASS) defines += "#define GBUFFER_PASS\n";

	FShader *shader = NULL;
	try
	{
		shader = new FShader(ShaderName);
		if (!shader->Load(ShaderName, "shaders/glsl/main.vp", "shaders/glsl/main.fp", ShaderPath, LightModePath, defines.GetChars()))
		{
			I_FatalError("Unable to load shader %s\n", ShaderName);
		}
	}
	catch(CRecoverableError &err)
	{
		if (shader != NULL) delete shader;
		shader = NULL;
		I_FatalError("Unable to load shader %s:\n%s\n", ShaderName, err.GetMessage());
	}
	return shader;
}

//==========================================================================
//
//
//
//==========================================================================

void FShader::ApplyMatrices(VSMatrix *proj, VSMatrix *view, VSMatrix *norm)
{
	Bind();
	glUniformMatrix4fv(projectionmatrix_index, 1, false, proj->get());
	glUniformMatrix4fv(viewmatrix_index, 1, false, view->get());
	glUniformMatrix4fv(normalviewmatrix_index, 1, false, norm->get());
}

//==========================================================================
//
//
//
//==========================================================================
struct FDefaultShader 
{
	const char * ShaderName;
	const char * gettexelfunc;
	const char * lightfunc;
	const char * Defines;
};

// Note: the MaterialShaderIndex enum in gl_shader.h needs to be updated whenever this array is modified.
static const FDefaultShader defaultshaders[]=
{	
	{"Default",	"shaders/glsl/func_normal.fp", "shaders/glsl/material_normal.fp", ""},
	{"Warp 1",	"shaders/glsl/func_warp1.fp", "shaders/glsl/material_normal.fp", ""},
	{"Warp 2",	"shaders/glsl/func_warp2.fp", "shaders/glsl/material_normal.fp", ""},
	{"Brightmap","shaders/glsl/func_brightmap.fp", "shaders/glsl/material_normal.fp", "#define BRIGHTMAP\n"},
	{"Specular", "shaders/glsl/func_spec.fp", "shaders/glsl/material_specular.fp", "#define SPECULAR\n#define NORMALMAP\n"},
	{"SpecularBrightmap", "shaders/glsl/func_spec.fp", "shaders/glsl/material_specular.fp", "#define SPECULAR\n#define NORMALMAP\n#define BRIGHTMAP\n"},
	{"PBR","shaders/glsl/func_pbr.fp", "shaders/glsl/material_pbr.fp", "#define PBR\n#define NORMALMAP\n"},
	{"PBRBrightmap","shaders/glsl/func_pbr.fp", "shaders/glsl/material_pbr.fp", "#define PBR\n#define NORMALMAP\n#define BRIGHTMAP\n"},
	{"No Texture", "shaders/glsl/func_notexture.fp", "shaders/glsl/material_normal.fp", ""},
	{"Basic Fuzz", "shaders/glsl/fuzz_standard.fp", "shaders/glsl/material_normal.fp", ""},
	{"Smooth Fuzz", "shaders/glsl/fuzz_smooth.fp", "shaders/glsl/material_normal.fp", ""},
	{"Swirly Fuzz", "shaders/glsl/fuzz_swirly.fp", "shaders/glsl/material_normal.fp", ""},
	{"Translucent Fuzz", "shaders/glsl/fuzz_smoothtranslucent.fp", "shaders/glsl/material_normal.fp", ""},
	{"Jagged Fuzz", "shaders/glsl/fuzz_jagged.fp", "shaders/glsl/material_normal.fp", ""},
	{"Noise Fuzz", "shaders/glsl/fuzz_noise.fp", "shaders/glsl/material_normal.fp", ""},
	{"Smooth Noise Fuzz", "shaders/glsl/fuzz_smoothnoise.fp", "shaders/glsl/material_normal.fp", ""},
	{"Software Fuzz", "shaders/glsl/fuzz_software.fp", "shaders/glsl/material_normal.fp", ""},
	{nullptr,nullptr,nullptr,nullptr}
};

TArray<UserShaderDesc> usershaders;

struct FEffectShader
{
	const char *ShaderName;
	const char *vp;
	const char *fp1;
	const char *fp2;
	const char *fp3;
	const char *defines;
};

static const FEffectShader effectshaders[]=
{
	{ "fogboundary", "shaders/glsl/main.vp", "shaders/glsl/fogboundary.fp", nullptr, nullptr, "#define NO_ALPHATEST\n" },
	{ "spheremap", "shaders/glsl/main.vp", "shaders/glsl/main.fp", "shaders/glsl/func_normal.fp", "shaders/glsl/material_normal.fp", "#define SPHEREMAP\n#define NO_ALPHATEST\n" },
	{ "burn", "shaders/glsl/main.vp", "shaders/glsl/burn.fp", nullptr, nullptr, "#define SIMPLE\n#define NO_ALPHATEST\n" },
	{ "stencil", "shaders/glsl/main.vp", "shaders/glsl/stencil.fp", nullptr, nullptr, "#define SIMPLE\n#define NO_ALPHATEST\n" },
};

FShaderManager::FShaderManager()
{
	if (!gl.legacyMode)
	{
		if (gl.es) // OpenGL ES does not support multiple fragment shader outputs. As a result, no GBUFFER passes are possible.
		{
			mPassShaders.Push(new FShaderCollection(NORMAL_PASS));
		}
		else
		{
			for (int passType = 0; passType < MAX_PASS_TYPES; passType++)
				mPassShaders.Push(new FShaderCollection((EPassType)passType));
		}
	}
}

FShaderManager::~FShaderManager()
{
	if (!gl.legacyMode)
	{
		glUseProgram(0);
		mActiveShader = NULL;

		for (auto collection : mPassShaders)
			delete collection;
	}
}

void FShaderManager::SetActiveShader(FShader *sh)
{
	if (mActiveShader != sh)
	{
		glUseProgram(sh!= NULL? sh->GetHandle() : 0);
		mActiveShader = sh;
	}
}

FShader *FShaderManager::BindEffect(int effect, EPassType passType)
{
	if (passType < mPassShaders.Size())
		return mPassShaders[passType]->BindEffect(effect);
	else
		return nullptr;
}

FShader *FShaderManager::Get(unsigned int eff, bool alphateston, EPassType passType)
{
	if (passType < mPassShaders.Size())
		return mPassShaders[passType]->Get(eff, alphateston);
	else
		return nullptr;
}

void FShaderManager::ApplyMatrices(VSMatrix *proj, VSMatrix *view, EPassType passType)
{
	if (gl.legacyMode)
	{
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(proj->get());
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf(view->get());
	}
	else
	{
		if (passType < mPassShaders.Size())
			mPassShaders[passType]->ApplyMatrices(proj, view);

		if (mActiveShader)
			mActiveShader->Bind();
	}
}

void FShaderManager::ResetFixedColormap()
{
	for (auto &collection : mPassShaders)
		collection->ResetFixedColormap();
}

//==========================================================================
//
//
//
//==========================================================================

FShaderCollection::FShaderCollection(EPassType passType)
{
	CompileShaders(passType);
}

//==========================================================================
//
//
//
//==========================================================================

FShaderCollection::~FShaderCollection()
{
	Clean();
}

//==========================================================================
//
//
//
//==========================================================================

void FShaderCollection::CompileShaders(EPassType passType)
{
	mMaterialShaders.Clear();
	mMaterialShadersNAT.Clear();
	for (int i = 0; i < MAX_EFFECTS; i++)
	{
		mEffectShaders[i] = NULL;
	}

	for(int i=0;defaultshaders[i].ShaderName != NULL;i++)
	{
		FShader *shc = Compile(defaultshaders[i].ShaderName, defaultshaders[i].gettexelfunc, defaultshaders[i].lightfunc, defaultshaders[i].Defines, true, passType);
		mMaterialShaders.Push(shc);
		if (i < SHADER_NoTexture)
		{
			FShader *shc = Compile(defaultshaders[i].ShaderName, defaultshaders[i].gettexelfunc, defaultshaders[i].lightfunc, defaultshaders[i].Defines, false, passType);
			mMaterialShadersNAT.Push(shc);
		}
	}

#ifdef __MOBILE__
    if( gl_customshader )
#endif
	for(unsigned i = 0; i < usershaders.Size(); i++)
	{
		FString name = ExtractFileBase(usershaders[i].shader);
		FString defines = defaultshaders[usershaders[i].shaderType].Defines + usershaders[i].defines;
		FShader *shc = Compile(name, usershaders[i].shader, defaultshaders[usershaders[i].shaderType].lightfunc, defines, true, passType);
		mMaterialShaders.Push(shc);
	}

	for(int i=0;i<MAX_EFFECTS;i++)
	{
		FShader *eff = new FShader(effectshaders[i].ShaderName);
		if (!eff->Load(effectshaders[i].ShaderName, effectshaders[i].vp, effectshaders[i].fp1,
						effectshaders[i].fp2, effectshaders[i].fp3, effectshaders[i].defines))
		{
			delete eff;
		}
		else mEffectShaders[i] = eff;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FShaderCollection::Clean()
{
	for (unsigned int i = 0; i < mMaterialShadersNAT.Size(); i++)
	{
		if (mMaterialShadersNAT[i] != NULL) delete mMaterialShadersNAT[i];
	}
	for (unsigned int i = 0; i < mMaterialShaders.Size(); i++)
	{
		if (mMaterialShaders[i] != NULL) delete mMaterialShaders[i];
	}
	for (int i = 0; i < MAX_EFFECTS; i++)
	{
		if (mEffectShaders[i] != NULL) delete mEffectShaders[i];
		mEffectShaders[i] = NULL;
	}
	mMaterialShaders.Clear();
	mMaterialShadersNAT.Clear();
}

//==========================================================================
//
//
//
//==========================================================================

int FShaderCollection::Find(const char * shn)
{
	FName sfn = shn;

	for(unsigned int i=0;i<mMaterialShaders.Size();i++)
	{
		if (mMaterialShaders[i]->mName == sfn)
		{
			return i;
		}
	}
	return -1;
}


//==========================================================================
//
//
//
//==========================================================================

FShader *FShaderCollection::BindEffect(int effect)
{
	if (effect >= 0 && effect < MAX_EFFECTS && mEffectShaders[effect] != NULL)
	{
		mEffectShaders[effect]->Bind();
		return mEffectShaders[effect];
	}
	return NULL;
}


//==========================================================================
//
//
//
//==========================================================================
EXTERN_CVAR(Int, gl_fuzztype)

void FShaderCollection::ApplyMatrices(VSMatrix *proj, VSMatrix *view)
{
	VSMatrix norm;
	norm.computeNormalMatrix(*view);

	for (int i = 0; i < SHADER_NoTexture; i++)
	{
		mMaterialShaders[i]->ApplyMatrices(proj, view, &norm);
		mMaterialShadersNAT[i]->ApplyMatrices(proj, view, &norm);
	}
	mMaterialShaders[SHADER_NoTexture]->ApplyMatrices(proj, view, &norm);
	if (gl_fuzztype != 0)
	{
		mMaterialShaders[SHADER_NoTexture + gl_fuzztype]->ApplyMatrices(proj, view, &norm);
	}
	for (unsigned i = FIRST_USER_SHADER; i < mMaterialShaders.Size(); i++)
	{
		mMaterialShaders[i]->ApplyMatrices(proj, view, &norm);
	}
	for (int i = 0; i < MAX_EFFECTS; i++)
	{
		mEffectShaders[i]->ApplyMatrices(proj, view, &norm);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void gl_DestroyUserShaders()
{
	// todo
}

static bool IsConsolePlayer(player_t *player)
{
	AActor *activator = player ? player->mo : nullptr;
	if (activator == nullptr || activator->player == nullptr)
		return false;
	return int(activator->player - players) == consoleplayer;
}

static void ShaderSetEnabled(player_t *player, const FString &shaderName, bool value)
{
	if (IsConsolePlayer(player))
	{
		for (unsigned int i = 0; i < PostProcessShaders.Size(); i++)
		{
			PostProcessShader &shader = PostProcessShaders[i];
			if (shader.Name == shaderName)
				shader.Enabled = value;
		}
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(_Shader, SetEnabled, ShaderSetEnabled)
{
	PARAM_PROLOGUE;
	PARAM_POINTER(player, player_t);
	PARAM_STRING(shaderName);
	PARAM_BOOL(value);
	ShaderSetEnabled(player, shaderName, value);

	return 0;
}

static void ShaderSetUniform1f(player_t *player, const FString &shaderName, const FString &uniformName, double value)
{
	if (IsConsolePlayer(player))
	{
		for (unsigned int i = 0; i < PostProcessShaders.Size(); i++)
		{
			PostProcessShader &shader = PostProcessShaders[i];
			if (shader.Name == shaderName)
			{
				double *vec4 = shader.Uniforms[uniformName].Values;
				vec4[0] = value;
				vec4[1] = 0.0;
				vec4[2] = 0.0;
				vec4[3] = 1.0;
			}
		}
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(_Shader, SetUniform1f, ShaderSetUniform1f)
{
	PARAM_PROLOGUE;
	PARAM_POINTER(player, player_t);
	PARAM_STRING(shaderName);
	PARAM_STRING(uniformName);
	PARAM_FLOAT(value);
	ShaderSetUniform1f(player, shaderName, uniformName, value);

	return 0;
}

DEFINE_ACTION_FUNCTION(_Shader, SetUniform2f)
{
	PARAM_PROLOGUE;
	PARAM_POINTER(player, player_t);
	PARAM_STRING(shaderName);
	PARAM_STRING(uniformName);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);

	if (IsConsolePlayer(player))
	{
		for (unsigned int i = 0; i < PostProcessShaders.Size(); i++)
		{
			PostProcessShader &shader = PostProcessShaders[i];
			if (shader.Name == shaderName)
			{
				double *vec4 = shader.Uniforms[uniformName].Values;
				vec4[0] = x;
				vec4[1] = y;
				vec4[2] = 0.0;
				vec4[3] = 1.0;
			}
		}
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(_Shader, SetUniform3f)
{
	PARAM_PROLOGUE;
	PARAM_POINTER(player, player_t);
	PARAM_STRING(shaderName);
	PARAM_STRING(uniformName);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);

	if (IsConsolePlayer(player))
	{
		for (unsigned int i = 0; i < PostProcessShaders.Size(); i++)
		{
			PostProcessShader &shader = PostProcessShaders[i];
			if (shader.Name == shaderName)
			{
				double *vec4 = shader.Uniforms[uniformName].Values;
				vec4[0] = x;
				vec4[1] = y;
				vec4[2] = z;
				vec4[3] = 1.0;
			}
		}
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(_Shader, SetUniform1i)
{
	PARAM_PROLOGUE;
	PARAM_POINTER(player, player_t);
	PARAM_STRING(shaderName);
	PARAM_STRING(uniformName);
	PARAM_INT(value);

	if (IsConsolePlayer(player))
	{
		for (unsigned int i = 0; i < PostProcessShaders.Size(); i++)
		{
			PostProcessShader &shader = PostProcessShaders[i];
			if (shader.Name == shaderName)
			{
				double *vec4 = shader.Uniforms[uniformName].Values;
				vec4[0] = (double)value;
				vec4[1] = 0.0;
				vec4[2] = 0.0;
				vec4[3] = 1.0;
			}
		}
	}
	return 0;
}
