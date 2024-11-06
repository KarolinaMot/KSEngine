#include "DXDevice.hpp"
#include <DXBackendModule.hpp>
#include <dxgi1_6.h>
#include <initialization/DXFactory.hpp>
#include <memory>

#if defined NDEBUG
constexpr bool DEBUG_MODE = false;
#else
constexpr bool DEBUG_MODE = true;
#endif

void DXBackendModule::Initialize(Engine& e)
{
    factory = std::make_unique<DXFactory>(DEBUG_MODE);

    auto selection_criteria = [](CD3DX12FeatureSupport features)
    {
        return features.MaxSupportedFeatureLevel() >= D3D_FEATURE_LEVEL_12_0 && features.RaytracingTier() >= D3D12_RAYTRACING_TIER_1_0;
    };

    device = std::make_unique<DXDevice>(
        factory->CreateDevice(DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, selection_criteria));
}

void DXBackendModule::Shutdown(Engine& e)
{
}
