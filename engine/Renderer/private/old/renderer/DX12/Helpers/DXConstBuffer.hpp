#pragma once
#include "DXResource.hpp"
#include <Common.hpp>
#include <DXCommon.hpp>
#include <memory>
#include <vector>


class DXCommandList;
class DXConstBuffer
{
public:
    DXConstBuffer() { };
    DXConstBuffer(const ComPtr<ID3D12Device5>& device, size_t dataSize, int numOfObjects, const char* bufferDebugName, int frameNumber);
    ~DXConstBuffer();

    void Update(const void* data, size_t dataSize, int offsetIndex, int frameIndex);
    void Bind(DXCommandList* commandList, int rootParameterIndex, int offsetIndex, int frameIndex) const;
    void Resize(const ComPtr<ID3D12Device5>& device, int newNumOfElements);
    size_t GetBufferPerObjectAlignedSize() { return mBufferPerObjectAlignedSize; }
    size_t GetGPUPointer(int slot, int bufferIndex);

private:
    std::vector<std::unique_ptr<DXResource>> mBuffers;
    UINT8* mBufferGPUAddress[2] = { 0, 0 };
    size_t mBufferPerObjectAlignedSize = 0;
    size_t mBufferSize = 0;
    int mNumOfElements = 0;
    bool inScope = false;
    const char* name;
};
