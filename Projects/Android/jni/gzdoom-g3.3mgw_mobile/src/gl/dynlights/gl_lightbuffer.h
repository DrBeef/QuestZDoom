#ifndef __GL_LIGHTBUFFER_H
#define __GL_LIGHTBUFFER_H

#include "tarray.h"
#include <atomic>
struct FDynLightData;

class FLightBuffer
{
	TArray<int> mIndices;
	unsigned int mBufferId;
	float * mBufferPointer;

	unsigned int mBufferType;
    std::atomic<unsigned int> mIndex;
	unsigned int mLastMappedIndex;
	unsigned int mBlockAlign;
	unsigned int mBlockSize;
	unsigned int mBufferSize;
	unsigned int mByteSize;
    unsigned int mMaxUploadSize;

public:

	FLightBuffer();
	~FLightBuffer();
	void Clear();
	int UploadLights(FDynLightData &data);
	void Begin();
	void Finish();
	int BindUBO(unsigned int index);
	void BindBase();
	unsigned int GetBlockSize() const { return mBlockSize; }
	unsigned int GetBufferType() const { return mBufferType; }
	unsigned int GetIndexPtr() const { return mIndices.Size();	}
	void StoreIndex(int index) { mIndices.Push(index); }
	int GetIndex(int i) const { return mIndices[i];	}
};

#endif

