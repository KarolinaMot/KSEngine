#pragma once
#include <code_utility.hpp>
#include <memory>
#include <renderer/DX12/Helpers/DXIncludes.hpp>

namespace KS
{

// Wait times of more than 5 secs not allowed!
constexpr DWORD MAX_TIMEOUT_PERIOD = 5 * 1000;

class GPUFence
{
public:
    GPUFence(ComPtr<ID3D12Device> device);
    ~GPUFence();

    void Signal(ComPtr<ID3D12CommandQueue> command_queue, uint64_t value);

    bool Reached(uint64_t time_point) const;
    void WaitFor(uint64_t time_point);

    NON_COPYABLE(GPUFence);
    NON_MOVABLE(GPUFence);

private:
    ComPtr<ID3D12Fence> fence_object {};
    HANDLE fence_wait_event;
};

class GPUFuture
{
public:
    GPUFuture() = default;

    GPUFuture(std::weak_ptr<GPUFence> fence, uint64_t target_val)
        : bound_fence(fence)
        , future_value(target_val)
    {
    }

    // Returns whether the Future is tracking an operation
    bool Valid() const;

    // Blocks the calling thread until the operation is finished
    void Wait();

    // Queries if the operation is finished (does not block)
    bool IsComplete() const;

private:
    std::weak_ptr<GPUFence> bound_fence {};
    uint64_t future_value {};
};

}