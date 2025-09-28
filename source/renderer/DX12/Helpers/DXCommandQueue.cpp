#include "DXCommandQueue.hpp"
#include "DXCommandList.hpp"
#include "DXCommandContextPool.hpp"

DXCommandQueue::DXCommandQueue(const ComPtr<ID3D12Device5>& device, const std::wstring& name)
{
    D3D12_COMMAND_QUEUE_DESC desc = {};

    CheckDX(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_command_queue)));
    m_command_queue->SetName(name.c_str());

    m_fence = std::make_shared<DXGPUFence>(device);
}

DXCommandQueue::~DXCommandQueue()
{
    m_fence->Signal(m_command_queue, ++m_next_fence_value);
    m_fence->WaitFor(m_next_fence_value);
}

DXGPUFuture DXCommandQueue::ExecuteCommandLists(ID3D12CommandList* const* ppCommandContexts,
                                                uint32_t commandContextsCount)
{
    std::lock_guard<std::mutex> lock(m_submitMutex);

    m_command_queue->ExecuteCommandLists(commandContextsCount, ppCommandContexts);
    m_fence->Signal(m_command_queue, ++m_next_fence_value);

    return DXGPUFuture(m_fence, m_next_fence_value);
}
