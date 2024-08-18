#include "DXGPUSync.hpp"

DXGPUFence::DXGPUFence(ComPtr<ID3D12Device> device)
{
    // Create fence
    CheckDX(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_object)));

    fence_wait_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    ASSERT(fence_wait_event && "Failed to create fence event.");
}

DXGPUFence::~DXGPUFence()
{
    CloseHandle(fence_wait_event);
}

void DXGPUFence::Signal(ComPtr<ID3D12CommandQueue> command_queue, uint64_t value)
{
    CheckDX(command_queue->Signal(fence_object.Get(), value));
}

bool DXGPUFence::Reached(uint64_t time_point) const
{
    return fence_object->GetCompletedValue() >= time_point;
}

void DXGPUFence::WaitFor(uint64_t time_point)
{
    if (fence_object->GetCompletedValue() < time_point)
    {
        CheckDX(
            fence_object->SetEventOnCompletion(time_point, fence_wait_event));

        if (WaitForSingleObject(fence_wait_event, MAX_TIMEOUT_PERIOD) == WAIT_TIMEOUT)
            CheckDX(-1);
    }
}

bool DXGPUFuture::Valid() const
{
    return !bound_fence.expired();
}

void DXGPUFuture::Wait()
{
    if (auto lock = bound_fence.lock())
    {
        lock->WaitFor(future_value);
    }
}

bool DXGPUFuture::IsComplete() const
{
    if (auto lock = bound_fence.lock())
    {
        return lock->Reached(future_value);
    }

    return true;
}
