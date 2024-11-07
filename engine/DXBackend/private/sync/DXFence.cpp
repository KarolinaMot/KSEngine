#include <cassert>
#include <sync/DXFence.hpp>

DXFence::DXFence(ID3D12Device* device)
{
    // Create fence
    CheckDX(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_object)));

    fence_wait_event = CreateEvent(NULL, FALSE, FALSE, NULL);
    assert(fence_wait_event && "Failed to create fence event.");
}

DXFence::~DXFence()
{
    CloseHandle(fence_wait_event);
}

void DXFence::Signal(ID3D12CommandQueue* command_queue, uint64_t value)
{
    CheckDX(command_queue->Signal(fence_object.Get(), value));
}

bool DXFence::Reached(uint64_t time_point) const
{
    return fence_object->GetCompletedValue() >= time_point;
}

void DXFence::WaitFor(uint64_t time_point)
{
    if (fence_object->GetCompletedValue() < time_point)
    {
        CheckDX(
            fence_object->SetEventOnCompletion(time_point, fence_wait_event));

        if (WaitForSingleObject(fence_wait_event, MAX_TIMEOUT_PERIOD) == WAIT_TIMEOUT)
            CheckDX(-1);
    }
}