#pragma once

#include <code_utility.hpp>
#include <dxgi1_6.h>
#include <functional>
#include <optional>
#include <renderer/DX12/Helpers/DXIncludes.hpp>

namespace KS
{

class DXFactory
{
public:
    DXFactory(bool debug);

    IDXGIFactory6* Handle() const { return factory.Get(); }

    bool SupportsTearing() const;

    std::optional<ComPtr<ID3D12Device>>
    CreateDevice(DXGI_GPU_PREFERENCE preference,
        std::function<bool(CD3DX12FeatureSupport)> feature_eval);

    NON_COPYABLE(DXFactory);
    NON_MOVABLE(DXFactory);

private:
    ComPtr<IDXGIFactory6> factory;

    // Debug only
    bool debug_mode {};
    ComPtr<ID3D12Debug> debug_interface;
};

} // namespace KS