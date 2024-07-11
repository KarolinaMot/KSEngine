#include "CommandQueue.hpp"

KS::CommandQueue::CommandQueue(const ComPtr<ID3D12Device5>& device, const std::wstring& name)
{
    D3D12_COMMAND_QUEUE_DESC desc = {};

    CheckDX(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_command_queue)));
    m_command_queue->SetName(name.c_str());

    m_fence = std::make_shared<GPUFence>(device);
}

KS::CommandQueue::~CommandQueue()
{
    Flush();
}

void KS::CommandQueue::Flush()
{
    m_fence->Signal(m_command_queue, ++m_next_fence_value);
    m_fence->WaitFor(m_next_fence_value);
}

KS::GPUFuture KS::CommandQueue::ExecuteCommandLists(ID3D12CommandList** ppCommandLists, uint32_t commandListCount)
{
    m_command_queue->ExecuteCommandLists(commandListCount, ppCommandLists);
    m_fence->Signal(m_command_queue, ++m_next_fence_value);

    return GPUFuture(m_fence, m_next_fence_value);
}

KS::GPUFuture KS::CommandQueue::ExecuteCommandList(ID3D12CommandList* command_list)
{
    return ExecuteCommandLists(&command_list, 1);
}
