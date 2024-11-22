#include <algorithm>
#include <commands/DXCommandAllocatorPool.hpp>

ID3D12CommandAllocator* DXCommandAllocatorPool::GetAllocator(ID3D12Device* device)
{
    auto find_free_slot = [](const CommandAllocatorSlot& s)
    {
        return s.info.in_use == false && s.info.complete_future.IsComplete();
    };

    auto it = std::find_if(available_allocators.begin(), available_allocators.end(), find_free_slot);
    if (it != available_allocators.end())
    {
        it->info = {};
        it->info.in_use = true;
        CheckDX(it->command_allocator->Reset());
        return it->command_allocator.Get();
    }
    else
    {
        ComPtr<ID3D12CommandAllocator> new_allocator {};
        CheckDX(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&new_allocator)));

        CommandAllocatorSlot new_slot {};
        new_slot.command_allocator = new_allocator;
        new_slot.info.in_use = true;

        available_allocators.emplace_back(new_slot);

        return new_allocator.Get();
    }
}

void DXCommandAllocatorPool::DiscardAllocator(ID3D12CommandAllocator* allocator, DXFuture future, std::vector<ComPtr<ID3D12Resource>>&& tracked_resources)
{
    auto find_discard_slot = [allocator](const CommandAllocatorSlot& s)
    {
        return s.command_allocator.Get() == allocator;
    };

    auto it = std::find_if(available_allocators.begin(), available_allocators.end(), find_discard_slot);

    if (it != available_allocators.end())
    {
        it->info.in_use = false;
        it->info.complete_future = future;
        it->info.tracked_resources = std::move(tracked_resources);
    }
}

void DXCommandAllocatorPool::FreeUnused()
{
    auto find_unused_slots = [](const CommandAllocatorSlot& s)
    {
        return s.info.in_use == false && s.info.complete_future.IsComplete();
    };

    auto it = std::remove_if(available_allocators.begin(), available_allocators.end(), find_unused_slots);
    available_allocators.erase(it, available_allocators.end());
}