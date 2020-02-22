#pragma once

#include "gl_shaderprogram.h"
#include <map>

struct PostProcessShader;

class PostProcessShaderInstance
{
public:
	PostProcessShaderInstance(PostProcessShader *desc) : Desc(desc) { }
	~PostProcessShaderInstance();

	void Run();

	PostProcessShader *Desc;

private:
	bool IsShaderSupported();
	void CompileShader();
	void UpdateUniforms();
	void BindTextures();

	FShaderProgram mProgram;
	FBufferedUniformSampler mInputTexture;
	std::map<FTexture*, int> mTextureHandles;
};

class FCustomPostProcessShaders
{
public:
	FCustomPostProcessShaders();
	~FCustomPostProcessShaders();

	void Run(FString target);

private:
	std::vector<std::unique_ptr<PostProcessShaderInstance>> mShaders;

	FCustomPostProcessShaders(const FCustomPostProcessShaders &) = delete;
	FCustomPostProcessShaders &operator=(const FCustomPostProcessShaders &) = delete;
};
