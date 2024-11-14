#pragma once
#include <Common.hpp>
#include <DXCommon.hpp>
#include <array>
#include <commands/DXCommandQueue.hpp>

class DXDevice
{
public:
    DXDevice(ComPtr<ID3D12Device> device);
    ~DXDevice() = default;

    ID3D12Device* Get() { return device.Get(); }
    DXCommandQueue& GetCommandQueue() { return *command_queue; }

private:
    ComPtr<ID3D12Device5> device;
    std::unique_ptr<DXCommandQueue> command_queue;
};