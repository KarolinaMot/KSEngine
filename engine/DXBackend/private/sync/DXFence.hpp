#pragma once
#include <Common.hpp>
#include <DXCommon.hpp>

class DXFence
{
public:
    static inline constexpr DWORD MAX_TIMEOUT_PERIOD = 5000;

    DXFence(ID3D12Device* device);
    ~DXFence();

    void Signal(ID3D12CommandQueue* command_queue, uint64_t value);

    NON_COPYABLE(DXFence);
    NON_MOVABLE(DXFence);

    bool Reached(uint64_t time_point) const;
    void WaitFor(uint64_t time_point);

private:
    ComPtr<ID3D12Fence> fence_object {};
    HANDLE fence_wait_event;
};