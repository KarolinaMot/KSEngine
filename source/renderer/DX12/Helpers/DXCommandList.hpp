
#pragma once
#include <memory>
#include <renderer/DX12/Helpers/DXIncludes.hpp>
#include <renderer/DX12/Helpers/DXHeapHandle.hpp>
#include <renderer/DX12/Helpers/DXDescHeap.hpp>

class DXCommandAllocator
{
public:
    DXCommandAllocator(ComPtr<ID3D12Device5> device, const char* name);
    ~DXCommandAllocator();
    void TrackResource(ComPtr<ID3D12Resource> buffer);
    void Reset();

    ComPtr<ID3D12CommandAllocator> GetAllocator() const;

private:
    std::vector<ComPtr<ID3D12Resource>> m_trackedResources;
    ComPtr<ID3D12CommandAllocator> m_allocator;
};

class DXCommandList
{
public:
    DXCommandList(ComPtr<ID3D12Device5> device, std::shared_ptr<DXCommandAllocator> allocator, const char* name);
    ~DXCommandList();

    void BindPipeline(ComPtr<ID3D12PipelineState> pipeline);
    void BindRootSignature(ComPtr<ID3D12RootSignature> signature, bool computePipeline = false);
    void BindDescriptorHeaps(DXDescHeap* rscHeap, DXDescHeap* rtHeap, DXDescHeap* depthHeap);
    void BindHeapResource(std::unique_ptr<DXResource>& resource, const DXHeapHandle& handle, int rootSlot);
    void BindRenderTargets(DXResource** rtResources, const DXHeapHandle* handles, std::unique_ptr<DXResource>& depthResource, const DXHeapHandle& dsvHandle, unsigned int numRtv = 1);
    void BindRenderTargets(std::unique_ptr<DXResource>& resource, const DXHeapHandle& rtvHeapSlot);
    void BindBuffer(const std::unique_ptr<DXResource>& resource, int rootParameter, size_t elementSize = 0, int offsetElement = 0);
    void ClearRenderTargets(std::unique_ptr<DXResource>& rtResource, const DXHeapHandle& handle, const float* clearData);
    void ClearDepthStencils(std::unique_ptr<DXResource>& depthResource, const DXHeapHandle& handle);
    void BindVertexData(const std::unique_ptr<DXResource>& buffer, size_t bufferStride, int inputSlot, int elementOffset);
    void BindIndexData(const std::unique_ptr<DXResource>& buffer, size_t bufferStride, int elementOffset);
    void DrawIndexed(int indexCount, int instancesCount = 1);
    void CopyResource(std::unique_ptr<DXResource>& source, std::unique_ptr<DXResource>& dest);
    void DispatchShader(uint32_t threadGroupX, uint32_t threadgGroupY, uint32_t threadGroupZ);
    void ResourceBarrier(ID3D12Resource& resource, D3D12_RESOURCE_STATES srcState, D3D12_RESOURCE_STATES dstState);
    void Open(std::shared_ptr<DXCommandAllocator> allocator);
    void TrackResource(ComPtr<ID3D12Resource> buffer);
    void Close();

    ComPtr<ID3D12GraphicsCommandList4> GetCommandList() const;

private:
    ComPtr<ID3D12GraphicsCommandList4> m_command_list;
    std::shared_ptr<DXCommandAllocator> m_allocator;

    ComPtr<ID3D12PipelineState> m_boundPipeline;
    bool m_isBoundSignatureCompute = false;
    ComPtr<ID3D12RootSignature> m_boundSignature;
    int m_boundDescHeapCount = 0;

    bool m_isOpen = false;
};