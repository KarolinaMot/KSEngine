
#pragma once

#include "GPUSync.hpp"
#include <memory>
#include <renderer/DX12/Helpers/DXIncludes.hpp>
#include <string>

namespace KS
{
class CommandQueue
{
public:
    CommandQueue(const ComPtr<ID3D12Device5>& device, const std::wstring& name);
    ~CommandQueue();

    // Waits the calling thread until all pending operations are executed
    void Flush();

    GPUFuture ExecuteCommandLists(ID3D12CommandList** ppCommandLists, uint32_t commandListCount);
    GPUFuture ExecuteCommandList(ID3D12CommandList* command_list);

    ID3D12CommandQueue* Get() const
    {
        return m_command_queue.Get();
    }

private:
    ComPtr<ID3D12CommandQueue> m_command_queue;
    std::shared_ptr<GPUFence> m_fence {};
    uint64_t m_next_fence_value = 0;
};
}