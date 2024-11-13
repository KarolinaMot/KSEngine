// #include "DXConstBuffer.hpp"
// #include "DXResource.hpp"
// #include "DXCommandList.hpp"
// #include <code_utility.hpp>

// DXConstBuffer::DXConstBuffer(const ComPtr<ID3D12Device5>& device, size_t dataSize, int numberOfObjects, const char* bufferDebugName, int frameNumber)
// {
//     mBufferPerObjectAlignedSize = (dataSize + 255) & ~255;
//     mNumOfElements = numberOfObjects;
//     mBufferSize = mBufferPerObjectAlignedSize * mNumOfElements;

//     mBuffers.resize(frameNumber);
//     name = bufferDebugName;

//     for (int i = 0; i < frameNumber; i++)
//     {
//         auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
//         auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(static_cast<UINT64>(mBufferSize));

//         mBuffers[i] = std::make_unique<DXResource>(device, heapProperties, resourceDesc, nullptr, bufferDebugName, D3D12_RESOURCE_STATE_GENERIC_READ);

//         CD3DX12_RANGE readRange(0, 0);
//         HRESULT hr = mBuffers[i]->GetResource()->Map(0, &readRange, reinterpret_cast<void**>(&mBufferGPUAddress[i]));
//         if (FAILED(hr))
//             ASSERT(false && "Buffer mapping failed");
//     }
// }

// DXConstBuffer::~DXConstBuffer()
// {
// }

// void DXConstBuffer::Update(const void* data, size_t dataSize, int offsetIndex, int frameIndex)
// {
//     // ASSERT_LOG(mBufferPerObjectAlignedSize * offsetIndex + dataSize <= mBufferSize, "No more room in buffer");
//     memcpy(mBufferGPUAddress[frameIndex] + (mBufferPerObjectAlignedSize * offsetIndex), data, dataSize);
// }

// void DXConstBuffer::Bind(DXCommandList* commandList, int rootParameterIndex, int offsetIndex, int frameIndex) const
// {
//     commandList->BindBuffer(mBuffers[frameIndex], rootParameterIndex, mBufferPerObjectAlignedSize, offsetIndex);
// }

// void DXConstBuffer::Resize(const ComPtr<ID3D12Device5>& device, int newNumOfElements)
// {
//     if (mNumOfElements == newNumOfElements)
//         return;

//     mNumOfElements = newNumOfElements;
//     mBufferSize = mBufferPerObjectAlignedSize * mNumOfElements;

//     for (int i = 0; i < mBuffers.size(); i++)
//     {
//         auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
//         auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(static_cast<UINT64>(mBufferSize));

//         mBuffers[i] = std::make_unique<DXResource>(device, heapProperties, resourceDesc, nullptr, name, D3D12_RESOURCE_STATE_GENERIC_READ);

//         CD3DX12_RANGE readRange(0, 0);
//         HRESULT hr = mBuffers[i]->GetResource()->Map(0, &readRange, reinterpret_cast<void**>(&mBufferGPUAddress[i]));
//         if (FAILED(hr))
//             ASSERT(false && "Buffer mapping failed");
//     }
// }

// const size_t DXConstBuffer::GetGPUPointer(int slot, int bufferIndex)
// {
//     return mBuffers[bufferIndex]->GetResource()->GetGPUVirtualAddress() + (mBufferPerObjectAlignedSize * slot);
// }
