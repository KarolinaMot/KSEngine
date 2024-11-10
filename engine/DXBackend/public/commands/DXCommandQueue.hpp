
#pragma once

#include <Common.hpp>
#include <commands/DXCommandAllocatorPool.hpp>
#include <commands/DXCommandList.hpp>
#include <memory>
#include <string>
#include <sync/DXFuture.hpp>

class DXCommandList;
class DXCommandQueue
{
public:
    constexpr static inline auto DEFAULT_NAME = L"Command Queue";

    DXCommandQueue(ID3D12Device* device, const wchar_t* name = DEFAULT_NAME);
    ~DXCommandQueue();

    NON_COPYABLE(DXCommandQueue);
    NON_MOVABLE(DXCommandQueue);

    ID3D12CommandQueue* Get() { return command_queue.Get(); }

    // Waits the calling thread until all pending operations are executed
    void Flush();

    DXCommandList MakeCommandList(ID3D12Device* device, const wchar_t* name = DXCommandList::DEFAULT_NAME);
    DXFuture SubmitCommandList(DXCommandList&& command_list);

private:
    ComPtr<ID3D12CommandQueue> command_queue {};
    std::unique_ptr<DXFence> fence {};
    std::unique_ptr<DXCommandAllocatorPool> allocator_pool {};
    uint64_t next_fence_value = 0;
};