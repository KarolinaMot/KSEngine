#pragma once

#include <Common.hpp>
#include <DXCommon.hpp>

#include <array>
#include <commands/DXCommandList.hpp>
#include <constants/MeshContants.hpp>
#include <gpu_resources/DXResource.hpp>
#include <resources/Mesh.hpp>

struct GPUMesh
{
    static constexpr size_t VertexBufferCount = MeshConstants::ATTRIBUTE_COUNT - 1;

    size_t index_count {};
    DXResource data_buffer {};
    std::array<D3D12_VERTEX_BUFFER_VIEW, VertexBufferCount> vertex_views {};
    D3D12_INDEX_BUFFER_VIEW index_view {};
};

namespace GPUMeshUtility
{

struct GPUMeshCreateResult
{
    GPUMesh mesh {};
    DXResource upload_buffer {};
};

GPUMeshCreateResult CreateGPUMesh(DXCommandList& upload_commands, ID3D12Device* device, const Mesh& mesh);
}