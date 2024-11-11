
#pragma once
#include <Common.hpp>
#include <DXCommon.hpp>
#include <descriptors/DXDescriptorHandle.hpp>
#include <glm/vec4.hpp>
#include <memory>

class DXCommandList
{
    friend class DXCommandQueue;

public:
    static constexpr inline auto DEFAULT_NAME = L"Command List";

    DXCommandList(ID3D12Device* device, ID3D12CommandAllocator* allocator, const wchar_t* name = DEFAULT_NAME);
    ~DXCommandList();

    NON_COPYABLE(DXCommandList);

    DXCommandList(DXCommandList&&);
    DXCommandList& operator=(DXCommandList&&);

    ID3D12GraphicsCommandList* Get() { return command_list.Get(); }

    void ClearRenderTarget(const DXDescriptorHandle& rtv_handle, const glm::vec4& clear_colour);
    void SetResourceBarriers(size_t num_barriers, const D3D12_RESOURCE_BARRIER* barriers);

    // void BindPipeline(ComPtr<ID3D12PipelineState> pipeline);
    // void BindRootSignature(ComPtr<ID3D12RootSignature> signature, bool computePipeline = false);
    // void BindDescriptorHeaps(DXDescHeap* rscHeap, DXDescHeap* rtHeap, DXDescHeap* depthHeap);
    // void BindHeapResource(std::unique_ptr<DXResource>& resource, const DXHeapHandle& handle, int rootSlot);
    // void BindRenderTargets(DXResource** rtResources, const DXHeapHandle* handles, std::unique_ptr<DXResource>& depthResource, const DXHeapHandle& dsvHandle, unsigned int numRtv = 1);
    // void BindRenderTargets(std::unique_ptr<DXResource>& resource, const DXHeapHandle& rtvHeapSlot);
    // void BindBuffer(const std::unique_ptr<DXResource>& resource, int rootParameter, size_t elementSize = 0, int offsetElement = 0);
    // void ClearRenderTargets(std::unique_ptr<DXResource>& rtResource, const DXHeapHandle& handle, const float* clearData);
    // void ClearDepthStencils(std::unique_ptr<DXResource>& depthResource, const DXHeapHandle& handle);
    // void BindVertexData(const std::unique_ptr<DXResource>& buffer, size_t bufferStride, int inputSlot, int elementOffset);
    // void BindIndexData(const std::unique_ptr<DXResource>& buffer, size_t bufferStride, int elementOffset);
    // void DrawIndexed(int indexCount, int instancesCount = 1);
    // void CopyResource(std::unique_ptr<DXResource>& source, std::unique_ptr<DXResource>& dest);
    // void DispatchShader(uint32_t threadGroupX, uint32_t threadgGroupY, uint32_t threadGroupZ);
    // void ResourceBarrier(ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES srcState, D3D12_RESOURCE_STATES dstState);
    // void Open(std::shared_ptr<DXCommandAllocator> allocator);
    // void Close();

private:
    ComPtr<ID3D12GraphicsCommandList> command_list {};
    std::vector<ComPtr<ID3D12Resource>> tracked_resources {};
    ID3D12CommandAllocator* parent_allocator;

    // ComPtr<ID3D12PipelineState> m_boundPipeline;
    // bool m_isBoundSignatureCompute = false;
    // ComPtr<ID3D12RootSignature> m_boundSignature;
    // int m_boundDescHeapCount = 0;

    // bool m_isOpen = false;
};