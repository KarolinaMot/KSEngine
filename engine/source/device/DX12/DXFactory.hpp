#pragma once

#include <code_utility.hpp>
#include <dxgi1_6.h>
#include <functional>
#include <renderer/DX12/Helpers/DXIncludes.hpp>

namespace KS {

class DXFactory {
public:
  DXFactory(bool debug) {

    debug = debug;
    UINT factory_flags{};

    // Enable debug layer
    if (debug) {
      factory_flags = DXGI_CREATE_FACTORY_DEBUG;

      CheckDX(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface)));
      debug_interface->EnableDebugLayer();
    }

    // Create factory
    CheckDX(CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(&factory)));
  }

  IDXGIFactory6 *Handle() const { return factory.Get(); }

  bool SupportsTearing() {

    BOOL tear_support{};
    if (FAILED(factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                            &tear_support, sizeof(BOOL))))
      return false;

    return tear_support;
  }

  ComPtr<ID3D12Device>
  CreateDevice(DXGI_GPU_PREFERENCE preference,
               std::function<bool(CD3DX12FeatureSupport)> feature_eval) {
    ComPtr<ID3D12Device> device;

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0;
         factory->EnumAdapterByGpuPreference(
             i, preference, IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND;
         ++i) {

      ComPtr<ID3D12Device> test_device;
      bool success =
          SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_1_0_CORE,
                                      IID_PPV_ARGS(&test_device)));

      if (success) {
        CD3DX12FeatureSupport features;
        features.Init(test_device.Get());
        if (feature_eval(features)) {
          device = test_device;
          break;
        }
      }
    }

    if (device) {
      if (debug_mode) {
        ComPtr<ID3D12InfoQueue> info_queue;
        if (SUCCEEDED(device.As(&info_queue))) {
          info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION,
                                         TRUE);
          info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
          info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
        }
      }

      return device;
    }

    throw std::runtime_error("Did not find suitable device");
  }

  NON_COPYABLE(DXFactory);
  NON_MOVABLE(DXFactory);

private:
  ComPtr<IDXGIFactory6> factory;

  // Debug only
  bool debug_mode{};
  ComPtr<ID3D12Debug> debug_interface;
};

} // namespace KS