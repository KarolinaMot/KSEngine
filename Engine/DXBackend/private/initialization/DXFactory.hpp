#pragma once

#include <Common.hpp>
#include <DXCommon.hpp>

#include <dxgi1_6.h>
#include <functional>

class DXFactory
{
public:
    DXFactory(bool enable_debug);
    IDXGIFactory6* Handle() const { return factory.Get(); }

    bool SupportsTearing() const;
    ComPtr<ID3D12Device5> CreateDevice(DXGI_GPU_PREFERENCE preference, std::function<bool(CD3DX12FeatureSupport)> feature_eval);

    NON_COPYABLE(DXFactory);
    NON_MOVABLE(DXFactory);

private:
    bool debug_enabled = false;
    ComPtr<IDXGIFactory6> factory;
    ComPtr<ID3D12Debug> debug_interface;

    static void CALLBACK debug_output_callback(
        D3D12_MESSAGE_CATEGORY Category,
        D3D12_MESSAGE_SEVERITY Severity,
        D3D12_MESSAGE_ID ID,
        LPCSTR pDescription,
        void* pContext);
};
