#pragma once

#include <Common.hpp>
#include <DXCommon.hpp>

#include <array>
#include <commands/DXCommandList.hpp>
#include <constants/MaterialConstants.hpp>
#include <gpu_resources/DXDescriptorHeap.hpp>
#include <gpu_resources/DXResource.hpp>
#include <rendering/InfoStructs.hpp>
#include <resources/Material.hpp>

// PBR Material for Graphics Rendering
struct GPUMaterial
{
    DXResource base_texture {};
    DXDescriptorHeap<SRV> texture_heap {};
};

namespace GPUMaterialUtility
{

struct GPUMaterialCreateResult
{
    GPUMaterial material {};
    DXResource upload_buffer {};
};

GPUMaterialCreateResult CreateGPUMaterial(DXCommandList& upload_commands, ID3D12Device* device, const Material& mesh);

}