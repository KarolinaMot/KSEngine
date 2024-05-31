#include "DXCommandQueue.hpp"
#include "../../../tools/Log.hpp"

KS::DXCommandQueue::DXCommandQueue(const ComPtr<ID3D12Device5> &device, LPCWSTR commandQueueName)
{
    D3D12_COMMAND_QUEUE_DESC cqDesc = {};
    HRESULT hr = device->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&m_command_queue));
    if (FAILED(hr))
    {
        LOG(Log::Severity::FATAL, "Failed to create command queue");
    }
    m_command_queue->SetName(commandQueueName);

    hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    if (FAILED(hr))
    {
        LOG(Log::Severity::FATAL, "Failed to create fence");
    }
    m_next_fence_value = 0;

    // CREATE FENCE EVENT
    m_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_fence_event == nullptr)
    {
        LOG(Log::Severity::FATAL, "Failed to create fence event");
    }
}

int KS::DXCommandQueue::ExecuteCommandLists(ID3D12CommandList **ppCommandLists, int commandListCount)
{
    m_command_queue->ExecuteCommandLists(commandListCount, ppCommandLists);
    return Signal();
}

void KS::DXCommandQueue::WaitForFenceValue(int value)
{
    if (m_fence->GetCompletedValue() < value)
    {
        if (FAILED(m_fence->SetEventOnCompletion(value, m_fence_event)))
        {
            LOG(Log::Severity::FATAL, "Failed to set fence event on completion.");
        }
        WaitForSingleObject(m_fence_event, INFINITE);
    }
}

int KS::DXCommandQueue::Signal()
{
    m_next_fence_value++;
    HRESULT hr = m_command_queue->Signal(m_fence.Get(), m_next_fence_value);

    if (FAILED(hr))
    {
        LOG(Log::Severity::FATAL, "Failed to signal fence");
    }

    return m_next_fence_value;
}