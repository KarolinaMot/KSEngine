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

void DXBackendModule::Initialize(MAYBE_UNUSED Engine& e)
{
    factory = std::make_unique<DXFactory>(DEBUG_MODE);

    // TODO: these requirements should be constants in DXCommon.hpp and be used on compiler / shader input files
    auto selection_criteria = [](CD3DX12FeatureSupport features)
    {
        bool feature_req = features.MaxSupportedFeatureLevel() >= D3D_FEATURE_LEVEL_12_0;
        bool raytracing_req = features.RaytracingTier() >= D3D12_RAYTRACING_TIER_1_0;
        bool root_signature_req = features.HighestRootSignatureVersion() >= D3D_ROOT_SIGNATURE_VERSION_1_1;
        bool shader_model_req = features.HighestShaderModel() >= D3D_SHADER_MODEL_6_3;

        return feature_req && raytracing_req && root_signature_req && shader_model_req;
    };

    device = std::make_unique<DXDevice>(
        factory->CreateDevice(DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, selection_criteria));

    shader_compiler = std::make_unique<DXShaderCompiler>();
}

void DXBackendModule::Shutdown(MAYBE_UNUSED Engine& e)
{
}
