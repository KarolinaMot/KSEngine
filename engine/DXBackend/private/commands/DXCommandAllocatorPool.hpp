#pragma once
#include <sync/DXFuture.hpp>
#include <vector>

// Manages a command allocators to be used for command lists
// TODO: this class is NOT thread safe
class DXCommandAllocatorPool
{
public:
    size_t GetAllocatorCount() const { return available_allocators.size(); }

    ID3D12CommandAllocator* GetAllocator(ID3D12Device* device);

    void DiscardAllocator(
        ID3D12CommandAllocator* allocator,
        DXFuture future,
        std::vector<ComPtr<ID3D12Resource>>&& tracked_resources);

    void FreeUnused();

private:
    struct TrackingData
    {
        bool in_use = true;
        DXFuture complete_future {};
        std::vector<ComPtr<ID3D12Resource>> tracked_resources {};
    };

    struct CommandAllocatorSlot
    {
        ComPtr<ID3D12CommandAllocator> command_allocator {};
        TrackingData info {};
    };

    std::vector<CommandAllocatorSlot> available_allocators {};
};