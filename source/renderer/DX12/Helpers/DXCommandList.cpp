#include <renderer/DX12/Helpers/DXCommandList.hpp>
#include <tools/Log.hpp>
#include "DXCommandList.hpp"

DXCommandAllocator::DXCommandAllocator(ComPtr<ID3D12Device5> device, const char* name)
{
    HRESULT hr = device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_allocator));
    if (FAILED(hr))
    {
        LOG(Log::Severity::FATAL, "Failed to create command allocator");
    }
}

DXCommandAllocator::~DXCommandAllocator()
{
    m_trackedResources.clear();
}

void DXCommandAllocator::TrackResource(ComPtr<ID3D12Resource> buffer)
{
    m_trackedResources.emplace_back(buffer);
}

void DXCommandAllocator::Reset()
{
    m_trackedResources.clear();
    if (FAILED(m_allocator->Reset()))
    {
        LOG(Log::Severity::FATAL, "Failed to reset command allocator");
    }
}

ComPtr<ID3D12CommandAllocator> DXCommandAllocator::GetAllocator() const
{
    return m_allocator;
}

DXCommandList::DXCommandList(ComPtr<ID3D12Device5> device, std::shared_ptr<DXCommandAllocator> allocator, const char* name)
{
    HRESULT hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        allocator->GetAllocator().Get(), NULL,
        IID_PPV_ARGS(&m_command_list));
    if (FAILED(hr))
    {
        LOG(Log::Severity::FATAL, "Failed to create command list");
    }

    wchar_t wString[4096];
    MultiByteToWideChar(CP_ACP, 0, name, -1, wString, 4096);
    m_command_list->SetName(wString);
    m_isOpen = true;
    m_allocator = allocator;
}

DXCommandList::~DXCommandList()
{
    if (m_isOpen)
        Close();
}

void DXCommandList::BindPipeline(ComPtr<ID3D12PipelineState> pipeline)
{
    if (!m_isOpen)
    {
        LOG(Log::Severity::WARN, "Cannot use command list which is closed. Command will be ignored.");
        return;
    }
    m_command_list->SetPipelineState(pipeline.Get());
    m_boundPipeline = pipeline;
}

void DXCommandList::BindRootSignature(ComPtr<ID3D12RootSignature> signature, bool computePipeline)
{
    if (!m_isOpen)
    {
        LOG(Log::Severity::WARN, "Cannot use command list which is closed. Command will be ignored.");
        return;
    }
    m_isBoundSignatureCompute = computePipeline;
    if (m_isBoundSignatureCompute)
        m_command_list->SetComputeRootSignature(signature.Get());
    else
        m_command_list->SetGraphicsRootSignature(signature.Get());

    m_boundSignature = signature;
}

void DXCommandList::BindDescriptorHeaps(DXDescHeap* rscHeap, DXDescHeap* rtHeap, DXDescHeap* depthHeap)
{
    if (!m_isOpen)
    {
        LOG(Log::Severity::WARN, "Cannot use command list which is closed. Command will be ignored.");
        return;
    }

    int heapCount = 0;
    if (rscHeap != nullptr)
        heapCount++;
    if (rtHeap != nullptr)
        heapCount++;
    if (depthHeap != nullptr)
        heapCount++;

    ID3D12DescriptorHeap* descriptorHeaps[] = { rscHeap->Get(), rtHeap ? rtHeap->Get() : nullptr, depthHeap ? depthHeap->Get() : nullptr };
    m_command_list->SetDescriptorHeaps(heapCount, descriptorHeaps);
}

void DXCommandList::BindHeapResource(std::unique_ptr<DXResource>& resource, const DXHeapHandle& handle, int rootSlot)
{
    if (!m_isOpen)
    {
        LOG(Log::Severity::WARN, "Cannot use command list which is closed. Command will be ignored.");
        return;
    }
    if (m_boundSignature == nullptr)
    {
        LOG(Log::Severity::WARN, "Cannot bind resource because there is no bound signature. Command has been ignored");
        return;
    }

    if (!handle.IsValid())
    {
        LOG(Log::Severity::WARN, "Cannot bind resource because the handle is not valid. Command has been ignored");
        return;
    }

    if (m_isBoundSignatureCompute)
        m_command_list->SetComputeRootDescriptorTable(rootSlot, handle.GetAddressGPU());
    else
        m_command_list->SetGraphicsRootDescriptorTable(rootSlot, handle.GetAddressGPU());

    m_allocator->TrackResource(resource->GetResource());
}

void DXCommandList::BindRenderTargets(DXResource** rtResources, const DXHeapHandle* handles, std::unique_ptr<DXResource>& depthResource, const DXHeapHandle& dsvHandle, unsigned int numRtv)
{
    if (!m_isOpen)
    {
        LOG(Log::Severity::WARN, "Cannot use command list which is closed. Command will be ignored.");
        return;
    }
    std::vector<CD3DX12_CPU_DESCRIPTOR_HANDLE> rtvHandles(numRtv);

    for (unsigned int i = 0; i < numRtv; i++)
    {
        if (!handles[i].IsValid())
        {
            LOG(Log::Severity::WARN, "Cannot bind render targets because handle {} is not valid. Command has been ignored", i);
            return;
        }
        rtvHandles[i] = handles[i].GetAddressCPU();
        m_allocator->TrackResource(rtResources[i]->GetResource());
    }

    if (!dsvHandle.IsValid())
    {
        LOG(Log::Severity::WARN, "Cannot bind render targets because depth handle is not valid. Command has been ignored");
        return;
    }

    m_allocator->TrackResource(depthResource->GetResource());

    CD3DX12_CPU_DESCRIPTOR_HANDLE depthHandle = dsvHandle.GetAddressCPU();
    m_command_list->OMSetRenderTargets(static_cast<UINT>(rtvHandles.size()), rtvHandles.data(), FALSE, &depthHandle);
}

void DXCommandList::BindRenderTargets(std::unique_ptr<DXResource>& rtResource, const DXHeapHandle& rtvHeapSlot)
{
    if (!m_isOpen)
    {
        LOG(Log::Severity::WARN, "Cannot use command list which is closed. Command will be ignored.");
        return;
    }
    if (!rtvHeapSlot.IsValid())
    {
        LOG(Log::Severity::WARN, "Cannot bind render target because handle is not valid. Command has been ignored");
        return;
    }
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtHandle = rtvHeapSlot.GetAddressCPU();
    m_command_list->OMSetRenderTargets(1, &rtHandle, FALSE, nullptr);
    m_allocator->TrackResource(rtResource->GetResource());
}

void DXCommandList::BindBuffer(const std::unique_ptr<DXResource>& resource, int rootParameter, size_t elementSize, int offsetElement)
{
    if (!m_isBoundSignatureCompute)
        m_command_list->SetGraphicsRootConstantBufferView(rootParameter, resource->GetResource()->GetGPUVirtualAddress() + (elementSize * offsetElement));
    else
        m_command_list->SetComputeRootConstantBufferView(rootParameter, resource->GetResource()->GetGPUVirtualAddress() + (elementSize * offsetElement));

    m_allocator->TrackResource(resource->GetResource());
}

void DXCommandList::ClearRenderTargets(std::unique_ptr<DXResource>& rtResource, const DXHeapHandle& handle, const float* clearData)
{
    if (!m_isOpen)
    {
        LOG(Log::Severity::WARN, "Cannot use command list which is closed. Command will be ignored.");
        return;
    }
    if (!handle.IsValid())
    {
        LOG(Log::Severity::WARN, "Cannot clear render target because handle is not valid. Command has been ignored");
        return;
    }

    m_command_list->ClearRenderTargetView(handle.GetAddressCPU(), clearData, 0, nullptr);
    m_allocator->TrackResource(rtResource->GetResource());
}

void DXCommandList::ClearDepthStencils(std::unique_ptr<DXResource>& depthResource, const DXHeapHandle& handle)
{
    if (!m_isOpen)
    {
        LOG(Log::Severity::WARN, "Cannot use command list which is closed. Command will be ignored.");
        return;
    }
    if (!handle.IsValid())
    {
        LOG(Log::Severity::WARN, "Cannot clear depth stencil because handle is not valid. Command has been ignored");
        return;
    }

    m_command_list->ClearDepthStencilView(handle.GetAddressCPU(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    m_allocator->TrackResource(depthResource->GetResource());
}

void DXCommandList::BindVertexData(const std::unique_ptr<DXResource>& buffer, size_t bufferStride, int inputSlot, int elementOffset)
{
    if (!m_isOpen)
    {
        LOG(Log::Severity::WARN, "Cannot use command list which is closed. Command will be ignored.");
        return;
    }

    ResourceBarrier(*buffer->Get(), buffer->GetState(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    buffer->ChangeState(D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    D3D12_VERTEX_BUFFER_VIEW vertexBufferView {};
    vertexBufferView.BufferLocation = buffer->Get()->GetGPUVirtualAddress() + elementOffset * bufferStride;
    vertexBufferView.StrideInBytes = bufferStride;
    vertexBufferView.SizeInBytes = buffer->GetResourceSize();

    m_command_list->IASetVertexBuffers(inputSlot, 1, &vertexBufferView);
    m_allocator->TrackResource(buffer->GetResource());
}

void DXCommandList::BindIndexData(const std::unique_ptr<DXResource>& buffer, size_t bufferStride, int elementOffset)
{
    if (!m_isOpen)
    {
        LOG(Log::Severity::WARN, "Cannot use command list which is closed. Command will be ignored.");
        return;
    }

    D3D12_INDEX_BUFFER_VIEW indexBufferView {};
    indexBufferView.BufferLocation = buffer->Get()->GetGPUVirtualAddress() + elementOffset * bufferStride;
    indexBufferView.SizeInBytes = buffer->GetResourceSize();

    ResourceBarrier(*buffer->Get(), buffer->GetState(), D3D12_RESOURCE_STATE_INDEX_BUFFER);
    buffer->ChangeState(D3D12_RESOURCE_STATE_INDEX_BUFFER);

    switch (bufferStride)
    {
    case sizeof(unsigned char):
        indexBufferView.Format = DXGI_FORMAT_R8_UINT;
        break;
    case sizeof(unsigned short):
        indexBufferView.Format = DXGI_FORMAT_R16_UINT;
        break;
    case sizeof(unsigned int):
        indexBufferView.Format = DXGI_FORMAT_R32_UINT;
        break;
    default:
        indexBufferView.Format = DXGI_FORMAT_R16_UINT;
        break;
    }

    m_command_list->IASetIndexBuffer(&indexBufferView);
    m_allocator->TrackResource(buffer->GetResource());
}

void DXCommandList::DrawIndexed(int indexCount, int instancesCount)
{
    if (!m_isOpen)
    {
        LOG(Log::Severity::WARN, "Cannot use command list which is closed. Command will be ignored.");
        return;
    }
    m_command_list->DrawIndexedInstanced(indexCount, instancesCount, 0, 0, 0);
}

void DXCommandList::CopyResource(std::unique_ptr<DXResource>& source, std::unique_ptr<DXResource>& dest)
{
    m_command_list->CopyResource(dest->Get(), source->Get());
    m_allocator->TrackResource(dest->GetResource());
    m_allocator->TrackResource(source->GetResource());
}

void DXCommandList::DispatchShader(uint32_t threadGroupX, uint32_t threadgGroupY, uint32_t threadGroupZ)
{
    if (!m_isOpen)
    {
        LOG(Log::Severity::WARN, "Cannot use command list which is closed. Command will be ignored.");
        return;
    }

    if (!m_isBoundSignatureCompute)
    {
        LOG(Log::Severity::WARN, "Cannot dispatch a shader when no compute shader is bound");
        return;
    }

    m_command_list->Dispatch(threadGroupX, threadgGroupY, threadGroupZ);
}

void DXCommandList::ResourceBarrier(ID3D12Resource& resource, D3D12_RESOURCE_STATES srcState, D3D12_RESOURCE_STATES dstState)
{
    if (!m_isOpen)
    {
        LOG(Log::Severity::WARN, "Cannot use command list which is closed. Command will be ignored.");
        return;
    }

    if (dstState == srcState)
        return;

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(&resource, srcState, dstState);
    m_command_list->ResourceBarrier(1, &barrier);
}

void DXCommandList::Open(std::shared_ptr<DXCommandAllocator> allocator)
{
    if (m_isOpen)
    {
        LOG(Log::Severity::WARN, "Command list cannot be opened because it is already open. Command will be ignored.");
        return;
    }

    if (FAILED(m_command_list->Reset(allocator->GetAllocator().Get(), nullptr)))
    {
        LOG(Log::Severity::FATAL, "Failed to reset command list");
    }

    m_allocator = allocator;
    m_isOpen = true;
}

void DXCommandList::TrackResource(ComPtr<ID3D12Resource> buffer)
{
    m_allocator->TrackResource(buffer);
}

void DXCommandList::Close()
{
    if (!m_isOpen)
    {
        LOG(Log::Severity::WARN, "Command list cannot be closed because it is not open. Command will be ignored.");
        return;
    }

    m_command_list->Close();
    m_isOpen = false;
}

ComPtr<ID3D12GraphicsCommandList4> DXCommandList::GetCommandList() const
{
    return m_command_list;
}
