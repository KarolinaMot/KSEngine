#include "DXFactory.hpp"
#include <tools/Log.hpp>

KS::DXFactory::DXFactory(bool debug)
{

    debug_mode = debug;
    UINT factory_flags {};

    // Enable debug layer
    if (debug)
    {
        factory_flags = DXGI_CREATE_FACTORY_DEBUG;

        CheckDX(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface)));
        debug_interface->EnableDebugLayer();
    }

    // Create factory
    CheckDX(CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(&factory)));
}

bool KS::DXFactory::SupportsTearing() const
{

    BOOL tear_support {};
    if (FAILED(factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
            &tear_support, sizeof(BOOL))))
        return false;

    return tear_support;
}

std::optional<ComPtr<ID3D12Device>> KS::DXFactory::CreateDevice(DXGI_GPU_PREFERENCE preference, std::function<bool(CD3DX12FeatureSupport)> feature_eval)
{
    ComPtr<ID3D12Device> device;

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0;
         factory->EnumAdapterByGpuPreference(
             i, preference, IID_PPV_ARGS(&adapter))
         != DXGI_ERROR_NOT_FOUND;
         ++i)
    {

        ComPtr<ID3D12Device> test_device;
        bool success = SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_1_0_CORE,
            IID_PPV_ARGS(&test_device)));

        if (success)
        {
            CD3DX12FeatureSupport features;
            features.Init(test_device.Get());
            if (feature_eval(features))
            {
                device = test_device;
                break;
            }
        }
    }

    if (device)
    {
        if (debug_mode)
        {
            ComPtr<ID3D12InfoQueue> info_queue;
            if (SUCCEEDED(device.As(&info_queue)))
            {
                info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION,
                    TRUE);
                info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
                info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
            }
        }

        return device;
    }

    LOG(Log::Severity::FATAL, "Failed to create DX12 Device, crash is emminent");
    return std::nullopt;
}
