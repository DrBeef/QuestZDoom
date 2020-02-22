// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2015 Christopher Bruns
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
** gl_present3dRowshader.h
** Final composition and present shader for row-interleaved stereoscopic 3D mode
**
*/

#ifndef GL_PRESENT3DROWSHADER_H_
#define GL_PRESENT3DROWSHADER_H_

#include "gl_shaderprogram.h"
#include "gl_presentshader.h"

class FPresentStereoShaderBase : public FPresentShaderBase
{
public:
	FBufferedUniformSampler LeftEyeTexture;
	FBufferedUniformSampler RightEyeTexture;
	FBufferedUniform1i WindowPositionParity;

protected:
	void Init(const char * vtx_shader_name, const char * program_name) override;
};

class FPresent3DCheckerShader : public FPresentStereoShaderBase
{
public:
	void Bind() override;
};

class FPresent3DColumnShader : public FPresentStereoShaderBase
{
public:
	void Bind() override;
};

class FPresent3DRowShader : public FPresentStereoShaderBase
{
public:
	void Bind() override;
};

// GL_PRESENT3DROWSHADER_H_
#endif