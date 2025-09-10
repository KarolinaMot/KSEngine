
#pragma once

#include <memory>
#include <renderer/DX12/Helpers/DXIncludes.hpp>
#include <renderer/DX12/Helpers/DXGPUSync.hpp>
#include <string>
#include <mutex>

struct DXCommandContext;
class DXCommandQueue
{
public:
    DXCommandQueue(const ComPtr<ID3D12Device5>& device, const std::wstring& name);
    ~DXCommandQueue();

    DXGPUFuture ExecuteCommandLists(ID3D12CommandList* const* ppCommandContexts, uint32_t commandContextsCount);

    ID3D12CommandQueue* Get() const
    {
        return m_command_queue.Get();
    }

private:
    ComPtr<ID3D12CommandQueue> m_command_queue;
    std::shared_ptr<DXGPUFence> m_fence {};

    std::mutex m_submitMutex;  
    uint64_t m_next_fence_value = 0;
};