#include <DXDevice.hpp>

DXDevice::DXDevice(ComPtr<ID3D12Device> _device)
{
    _device.As(&device);
    command_queue = std::make_unique<DXCommandQueue>(device.Get(), L"Main command queue");
}