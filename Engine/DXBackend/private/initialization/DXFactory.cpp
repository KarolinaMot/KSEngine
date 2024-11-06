#include "Common.hpp"
#include "directx/d3d12sdklayers.h"
#include <Log.hpp>
#include <d3d12sdklayers.h>
#include <initialization/DXFactory.hpp>

DXFactory::DXFactory(bool debug)
{
    UINT factory_flags {};

    // Enable debug layer
    if (debug)
    {
        factory_flags = DXGI_CREATE_FACTORY_DEBUG;
        debug_enabled = true;

        CheckDX(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface)));
        debug_interface->EnableDebugLayer();
    }

    // Create factory
    CheckDX(CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(&factory)));
}

bool DXFactory::SupportsTearing() const
{
    BOOL tear_support {};
    if (FAILED(factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
            &tear_support, sizeof(BOOL))))
        return false;

    return tear_support;
}

ComPtr<ID3D12Device> DXFactory::CreateDevice(DXGI_GPU_PREFERENCE preference, std::function<bool(CD3DX12FeatureSupport)> feature_eval)
{
    ComPtr<ID3D12Device> device;

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapterByGpuPreference(i, preference, IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND; ++i)
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
        if (debug_enabled)
        {
            ComPtr<ID3D12InfoQueue1> info_queue;
            if (SUCCEEDED(device.As(&info_queue)))
            {
                info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
                info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);

                info_queue->RegisterMessageCallback(
                    debug_output_callback,
                    D3D12_MESSAGE_CALLBACK_FLAG_NONE,
                    nullptr, nullptr);
            }
            else
            {
                Log("Failed to enable debug layers");
            }
        }
        return device;
    }

    Log("Failed to create DX12 Device");
    return nullptr;
}

void CALLBACK DXFactory::debug_output_callback(
    D3D12_MESSAGE_CATEGORY Category,
    D3D12_MESSAGE_SEVERITY Severity,
    D3D12_MESSAGE_ID ID,
    LPCSTR pDescription,
    MAYBE_UNUSED void* pContext)
{
    auto MessageCategory = [](D3D12_MESSAGE_CATEGORY Category)
    {
        switch (Category)
        {
        case D3D12_MESSAGE_CATEGORY_APPLICATION_DEFINED:
            return "Application Defined";
        case D3D12_MESSAGE_CATEGORY_CLEANUP:
            return "Cleanup";
        case D3D12_MESSAGE_CATEGORY_COMPILATION:
            return "Compilation";
        case D3D12_MESSAGE_CATEGORY_EXECUTION:
            return "Execution";
        case D3D12_MESSAGE_CATEGORY_INITIALIZATION:
            return "Initialization";
        case D3D12_MESSAGE_CATEGORY_MISCELLANEOUS:
            return "Miscellaneous";
        case D3D12_MESSAGE_CATEGORY_RESOURCE_MANIPULATION:
            return "Resource Manipulation";
        case D3D12_MESSAGE_CATEGORY_SHADER:
            return "Shader";
        case D3D12_MESSAGE_CATEGORY_STATE_CREATION:
            return "State Creation";
        case D3D12_MESSAGE_CATEGORY_STATE_GETTING:
            return "State Getting";
        case D3D12_MESSAGE_CATEGORY_STATE_SETTING:
            return "State Setting";
        }
    };

    auto MessageSeverity = [](D3D12_MESSAGE_SEVERITY Category)
    {
        switch (Category)
        {
        case D3D12_MESSAGE_SEVERITY_CORRUPTION:
            return "Corruption";
        case D3D12_MESSAGE_SEVERITY_ERROR:
            return "Error";
        case D3D12_MESSAGE_SEVERITY_WARNING:
            return "Warning";
        case D3D12_MESSAGE_SEVERITY_INFO:
            return "Info";
        case D3D12_MESSAGE_SEVERITY_MESSAGE:
            return "Message";
        }
    };

    Log("D3D12 {} [{}]: {} (Error code: {})", MessageSeverity(Severity), MessageCategory(Category), pDescription, ID);
}
