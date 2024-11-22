#include <commands/DXCommandList.hpp>

DXCommandList::DXCommandList(ID3D12Device* device, ID3D12CommandAllocator* allocator, const wchar_t* name)
{
    CheckDX(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, NULL, IID_PPV_ARGS(&command_list)));
    command_list->SetName(name);
    parent_allocator = allocator;
}

DXCommandList::~DXCommandList()
{
    if (command_list != nullptr)
    {
        CheckDX(command_list->Close());
    }
}

DXCommandList::DXCommandList(DXCommandList&& other)
{
    command_list = other.command_list;
    other.command_list = nullptr;
    parent_allocator = other.parent_allocator;
    other.parent_allocator = nullptr;
    tracked_resources = std::move(other.tracked_resources);
}

DXCommandList& DXCommandList::operator=(DXCommandList&& other)
{
    if (&other == this)
        return *this;

    if (command_list != nullptr)
    {
        CheckDX(command_list->Close());
    }

    command_list = other.command_list;
    other.command_list = nullptr;
    parent_allocator = other.parent_allocator;
    other.parent_allocator = nullptr;
    tracked_resources = std::move(other.tracked_resources);

    return *this;
}

void DXCommandList::ClearRenderTarget(CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle, const glm::vec4& clear_colour)
{
    float colour[4] = { clear_colour.x, clear_colour.y, clear_colour.z, clear_colour.w };
    command_list->ClearRenderTargetView(rtv_handle, colour, 0, nullptr);
}

void DXCommandList::ClearDepthStencil(CD3DX12_CPU_DESCRIPTOR_HANDLE dsv_handle, float depth, uint32_t stencil)
{
    command_list->ClearDepthStencilView(
        dsv_handle,
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
        depth, stencil, 0, nullptr);
}

void DXCommandList::SetResourceBarriers(size_t num_barriers, const D3D12_RESOURCE_BARRIER* barriers)
{
    command_list->ResourceBarrier(num_barriers, barriers);
}

void DXCommandList::BindRootCBV(const DXResource& resource, uint32_t index)
{
    command_list->SetGraphicsRootConstantBufferView(index, resource.GetAddress());
}

void DXCommandList::CopyBuffer(DXResource& source, size_t source_offset, DXResource& destination, size_t destination_offset, size_t count)
{
    command_list->CopyBufferRegion(destination.Get(), destination_offset, source.Get(), source_offset, count);
}

// void DXCommandList::BindPipeline(ComPtr<ID3D12PipelineState> pipeline)
// {
//     if (!m_isOpen)
//     {
//         Log("Cannot use command list which is closed. Command will be ignored.");
//         return;
//     }
//     m_command_list->SetPipelineState(pipeline.Get());
//     m_boundPipeline = pipeline;
// }

// void DXCommandList::BindRootSignature(ComPtr<ID3D12RootSignature> signature, bool computePipeline)
// {
//     if (!m_isOpen)
//     {
//         Log("Cannot use command list which is closed. Command will be ignored.");
//         return;
//     }
//     m_isBoundSignatureCompute = computePipeline;
//     if (m_isBoundSignatureCompute)
//         m_command_list->SetComputeRootSignature(signature.Get());
//     else
//         m_command_list->SetGraphicsRootSignature(signature.Get());

//     m_boundSignature = signature;
// }

// void DXCommandList::BindDescriptorHeaps(DXDescHeap* rscHeap, DXDescHeap* rtHeap, DXDescHeap* depthHeap)
// {
//     if (!m_isOpen)
//     {
//         Log("Cannot use command list which is closed. Command will be ignored.");
//         return;
//     }

//     int heapCount = 0;
//     if (rscHeap != nullptr)
//         heapCount++;
//     if (rtHeap != nullptr)
//         heapCount++;
//     if (depthHeap != nullptr)
//         heapCount++;

//     ID3D12DescriptorHeap* descriptorHeaps[] = { rscHeap->Get(), rtHeap ? rtHeap->Get() : nullptr, depthHeap ? depthHeap->Get() : nullptr };
//     m_command_list->SetDescriptorHeaps(heapCount, descriptorHeaps);
// }

// void DXCommandList::BindHeapResource(std::unique_ptr<DXResource>& resource, const DXHeapHandle& handle, int rootSlot)
// {
//     if (!m_isOpen)
//     {
//         Log("Cannot use command list which is closed. Command will be ignored.");
//         return;
//     }
//     if (m_boundSignature == nullptr)
//     {
//         Log("Cannot bind resource because there is no bound signature. Command has been ignored");
//         return;
//     }

//     if (!handle.IsValid())
//     {
//         Log("Cannot bind resource because the handle is not valid. Command has been ignored");
//         return;
//     }

//     if (m_isBoundSignatureCompute)
//         m_command_list->SetComputeRootDescriptorTable(rootSlot, handle.GetAddressGPU());
//     else
//         m_command_list->SetGraphicsRootDescriptorTable(rootSlot, handle.GetAddressGPU());

//     m_allocator->TrackResource(resource->GetResource());
// }

// void DXCommandList::BindRenderTargets(DXResource** rtResources, const DXHeapHandle* handles, std::unique_ptr<DXResource>& depthResource, const DXHeapHandle& dsvHandle, unsigned int numRtv)
// {
//     if (!m_isOpen)
//     {
//         Log("Cannot use command list which is closed. Command will be ignored.");
//         return;
//     }
//     std::vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> rtvHandles(numRtv);

//     for (unsigned int i = 0; i < numRtv; i++)
//     {
//         if (!handles[i].IsValid())
//         {
//             Log("Cannot bind render targets because handle {} is not valid. Command has been ignored", i);
//             return;
//         }
//         rtvHandles[i] = handles[i].GetAddressCPU();
//         m_allocator->TrackResource(rtResources[i]->GetResource());
//     }

//     if (!dsvHandle.IsValid())
//     {
//         Log("Cannot bind render targets because depth handle is not valid. Command has been ignored");
//         return;
//     }

//     m_allocator->TrackResource(depthResource->GetResource());

//     CD3DX12_CPU_DESCRIPTOR_HANDLE depthHandle = dsvHandle.GetAddressCPU();
//     m_command_list->OMSetRenderTargets(static_cast<UINT>(rtvHandles.size()), rtvHandles.data(), FALSE, &depthHandle);
// }

// void DXCommandList::BindRenderTargets(std::unique_ptr<DXResource>& rtResource, const DXHeapHandle& rtvHeapSlot)
// {
//     if (!m_isOpen)
//     {
//         Log("Cannot use command list which is closed. Command will be ignored.");
//         return;
//     }
//     if (!rtvHeapSlot.IsValid())
//     {
//         Log("Cannot bind render target because handle is not valid. Command has been ignored");
//         return;
//     }
//     CD3DX12_CPU_DESCRIPTOR_HANDLE rtHandle = rtvHeapSlot.GetAddressCPU();
//     m_command_list->OMSetRenderTargets(1, &rtHandle, FALSE, nullptr);
//     m_allocator->TrackResource(rtResource->GetResource());
// }

// void DXCommandList::BindBuffer(const std::unique_ptr<DXResource>& resource, int rootParameter, size_t elementSize, int offsetElement)
// {
//     if (!m_isBoundSignatureCompute)
//         m_command_list->SetGraphicsRootConstantBufferView(rootParameter, resource->GetResource()->GetGPUVirtualAddress() + (elementSize * offsetElement));
//     else
//         m_command_list->SetComputeRootConstantBufferView(rootParameter, resource->GetResource()->GetGPUVirtualAddress() + (elementSize * offsetElement));

//     m_allocator->TrackResource(resource->GetResource());
// }

// void DXCommandList::ClearRenderTargets(std::unique_ptr<DXResource>& rtResource, const DXHeapHandle& handle, const float* clearData)
// {
//     if (!m_isOpen)
//     {
//         Log("Cannot use command list which is closed. Command will be ignored.");
//         return;
//     }
//     if (!handle.IsValid())
//     {
//         Log("Cannot clear render target because handle is not valid. Command has been ignored");
//         return;
//     }

//     m_command_list->ClearRenderTargetView(handle.GetAddressCPU(), clearData, 0, nullptr);
//     m_allocator->TrackResource(rtResource->GetResource());
// }

// void DXCommandList::ClearDepthStencils(std::unique_ptr<DXResource>& depthResource, const DXHeapHandle& handle)
// {
//     if (!m_isOpen)
//     {
//         Log("Cannot use command list which is closed. Command will be ignored.");
//         return;
//     }
//     if (!handle.IsValid())
//     {
//         Log("Cannot clear depth stencil because handle is not valid. Command has been ignored");
//         return;
//     }

//     m_command_list->ClearDepthStencilView(handle.GetAddressCPU(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
//     m_allocator->TrackResource(depthResource->GetResource());
// }

// void DXCommandList::BindVertexData(const std::unique_ptr<DXResource>& buffer, size_t bufferStride, int inputSlot, int elementOffset)
// {
//     if (!m_isOpen)
//     {
//         Log("Cannot use command list which is closed. Command will be ignored.");
//         return;
//     }

//     D3D12_VERTEX_BUFFER_VIEW vertexBufferView {};
//     vertexBufferView.BufferLocation = buffer->Get()->GetGPUVirtualAddress() + elementOffset * bufferStride;
//     vertexBufferView.StrideInBytes = bufferStride;
//     vertexBufferView.SizeInBytes = buffer->GetResourceSize();

//     m_command_list->IASetVertexBuffers(inputSlot, 1, &vertexBufferView);
//     m_allocator->TrackResource(buffer->GetResource());
// }

// void DXCommandList::BindIndexData(const std::unique_ptr<DXResource>& buffer, size_t bufferStride, int elementOffset)
// {
//     if (!m_isOpen)
//     {
//         Log("Cannot use command list which is closed. Command will be ignored.");
//         return;
//     }

//     D3D12_INDEX_BUFFER_VIEW indexBufferView {};
//     indexBufferView.BufferLocation = buffer->Get()->GetGPUVirtualAddress() + elementOffset * bufferStride;
//     indexBufferView.SizeInBytes = buffer->GetResourceSize();

//     switch (bufferStride)
//     {
//     case sizeof(unsigned char):
//         indexBufferView.Format = DXGI_FORMAT_R8_UINT;
//         break;
//     case sizeof(unsigned short):
//         indexBufferView.Format = DXGI_FORMAT_R16_UINT;
//         break;
//     case sizeof(unsigned int):
//         indexBufferView.Format = DXGI_FORMAT_R32_UINT;
//         break;
//     default:
//         indexBufferView.Format = DXGI_FORMAT_R16_UINT;
//         break;
//     }

//     m_command_list->IASetIndexBuffer(&indexBufferView);
//     m_allocator->TrackResource(buffer->GetResource());
// }

// void DXCommandList::DrawIndexed(int indexCount, int instancesCount)
// {
//     if (!m_isOpen)
//     {
//         Log("Cannot use command list which is closed. Command will be ignored.");
//         return;
//     }
//     m_command_list->DrawIndexedInstanced(indexCount, instancesCount, 0, 0, 0);
// }

// void DXCommandList::CopyResource(std::unique_ptr<DXResource>& source, std::unique_ptr<DXResource>& dest)
// {
//     m_command_list->CopyResource(dest->Get(), source->Get());
//     m_allocator->TrackResource(dest->GetResource());
//     m_allocator->TrackResource(source->GetResource());
// }

// void DXCommandList::DispatchShader(uint32_t threadGroupX, uint32_t threadgGroupY, uint32_t threadGroupZ)
// {
//     if (!m_isOpen)
//     {
//         Log("Cannot use command list which is closed. Command will be ignored.");
//         return;
//     }

//     if (!m_isBoundSignatureCompute)
//     {
//         Log("Cannot dispatch a shader when no compute shader is bound");
//         return;
//     }

//     m_command_list->Dispatch(threadGroupX, threadgGroupY, threadGroupZ);
// }

// void DXCommandList::ResourceBarrier(ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES srcState, D3D12_RESOURCE_STATES dstState)
// {
//     if (!m_isOpen)
//     {
//         Log("Cannot use command list which is closed. Command will be ignored.");
//         return;
//     }

//     if (dstState == srcState)
//         return;

//     auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), srcState, dstState);
//     m_command_list->ResourceBarrier(1, &barrier);
// }

// void DXCommandList::Open(std::shared_ptr<DXCommandAllocator> allocator)
// {
//     if (m_isOpen)
//     {
//         Log("Command list cannot be opened because it is already open. Command will be ignored.");
//         return;
//     }

//     if (FAILED(m_command_list->Reset(allocator->GetAllocator().Get(), nullptr)))
//     {
//         Log("Failed to reset command list");
//     }

//     m_allocator = allocator;
//     m_isOpen = true;
// }

// void DXCommandList::Close()
// {
//     if (!m_isOpen)
//     {
//         Log("Command list cannot be closed because it is not open. Command will be ignored.");
//         return;
//     }

//     m_command_list->Close();
//     m_isOpen = false;
// }

// ComPtr<ID3D12GraphicsCommandList4> DXCommandList::GetCommandList() const
// {
//     return m_command_list;
// }
