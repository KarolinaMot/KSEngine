
#pragma once
#include "DXIncludes.hpp"

namespace KS
{
    class DXCommandQueue
    {
    public:
        DXCommandQueue(const ComPtr<ID3D12Device5> &device, LPCWSTR commandQueueName);
        int ExecuteCommandLists(ID3D12CommandList **ppCommandLists, int commandListCount);
        void WaitForFenceValue(int value);

        ComPtr<ID3D12CommandQueue> GetCommandQueue() const { return m_command_queue; }
        ID3D12CommandQueue *Get() const { return m_command_queue.Get(); }

    private:
        int Signal();

        ComPtr<ID3D12CommandQueue> m_command_queue;

        ComPtr<ID3D12Fence> m_fence;
        int m_next_fence_value = -1;
        HANDLE m_fence_event;
    };
}