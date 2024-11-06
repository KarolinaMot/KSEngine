#pragma once
#include <Common.hpp>
#include <DXCommon.hpp>

class DXDevice
{
public:
    DXDevice(ComPtr<ID3D12Device> device);
    ~DXDevice() = default;
};