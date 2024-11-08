#include <commands/DXCommandQueue.hpp>

DXCommandQueue::DXCommandQueue(ID3D12Device* device, const wchar_t* name)
{
    D3D12_COMMAND_QUEUE_DESC desc = {};

    CheckDX(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&command_queue)));
    command_queue->SetName(name);

    fence = std::make_unique<DXFence>(device);
}

DXCommandQueue::~DXCommandQueue()
{
    Flush();
}

void DXCommandQueue::Flush()
{
    fence->Signal(command_queue.Get(), ++next_fence_value);
    fence->WaitFor(next_fence_value);
}

DXCommandList DXCommandQueue::MakeCommandList(ID3D12Device* device, const wchar_t* name)
{
    auto* allocator = allocator_pool->GetAllocator(device);
    return DXCommandList(device, allocator, name);
}

DXFuture DXCommandQueue::SubmitCommandList(DXCommandList&& command_list)
{
    CheckDX(command_list.command_list->Close());

    ID3D12CommandList* pList = command_list.command_list.Get();
    command_queue->ExecuteCommandLists(1, &pList);

    command_list.command_list = nullptr;

    fence->Signal(command_queue.Get(), ++next_fence_value);
    auto result_future = DXFuture(fence.get(), next_fence_value);

    allocator_pool->DiscardAllocator(command_list.parent_allocator, result_future, std::move(command_list.tracked_resources));
    return DXFuture(fence.get(), next_fence_value);
}
