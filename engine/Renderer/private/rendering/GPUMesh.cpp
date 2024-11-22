#include <Log.hpp>
#include <gpu_resources/DXResourceBuilder.hpp>
#include <rendering/GPUMesh.hpp>

GPUMeshUtility::GPUMeshCreateResult GPUMeshUtility::CreateGPUMesh(DXCommandList& upload_commands, ID3D12Device* device, const Mesh& mesh)
{
    size_t mesh_data_size {};

    for (size_t i = 0; i < MeshConstants::ATTRIBUTE_COUNT; i++)
    {
        if (auto* buffer = mesh.GetAttribute(MeshConstants::AttributeNames[i]))
        {
            mesh_data_size += buffer->GetSize();
        }
        else
        {
            Log("GPUMesh creation Error: {} data is not present in input mesh", MeshConstants::AttributeNames[i]);
            return {};
        }
    }

    DXResourceBuilder builder {};
    builder
        .WithHeapType(D3D12_HEAP_TYPE_UPLOAD)
        .WithInitialState(D3D12_RESOURCE_STATE_COPY_SOURCE);

    auto upload_buffer = builder.MakeBuffer(device, mesh_data_size).value();

    builder
        .WithHeapType(D3D12_HEAP_TYPE_DEFAULT)
        .WithInitialState(D3D12_RESOURCE_STATE_COPY_DEST);

    auto mesh_buffer = builder.MakeBuffer(device, mesh_data_size).value();

    auto mapped_ptr = upload_buffer.Map(0);

    std::array<D3D12_VERTEX_BUFFER_VIEW, GPUMesh::VertexBufferCount> vertex_views;
    D3D12_INDEX_BUFFER_VIEW index_view {};

    size_t index_count = 0;
    size_t data_ptr = 0;

    // Index Buffer
    {
        auto* index_byte_buffer = mesh.GetAttribute(MeshConstants::AttributeNames[0]);

        index_view.BufferLocation = mesh_buffer.GetAddress() + data_ptr;
        index_view.SizeInBytes = index_byte_buffer->GetSize();
        index_view.Format = DXGI_FORMAT_R32_UINT;

        index_count = index_byte_buffer->GetView<uint32_t>().count();

        std::memcpy(mapped_ptr.Get() + data_ptr, index_byte_buffer->GetData(), index_byte_buffer->GetSize());
        data_ptr += index_byte_buffer->GetSize();
    }

    // Vertex Buffers
    for (size_t i = 1; i < MeshConstants::ATTRIBUTE_COUNT; i++)
    {
        auto* vertex_byte_buffer = mesh.GetAttribute(MeshConstants::AttributeNames[i]);

        vertex_views.at(i - 1).BufferLocation = mesh_buffer.GetAddress() + data_ptr;
        vertex_views.at(i - 1).SizeInBytes = vertex_byte_buffer->GetSize();
        vertex_views.at(i - 1).StrideInBytes = MeshConstants::AttributeStrides[i];

        std::memcpy(mapped_ptr.Get() + data_ptr, vertex_byte_buffer->GetData(), vertex_byte_buffer->GetSize());
        data_ptr += vertex_byte_buffer->GetSize();
    }

    assert(data_ptr == mesh_data_size && "Mismatch between total mesh data size and mesh data updated!");
    upload_buffer.Unmap(std::move(mapped_ptr), D3D12_RANGE { 0, mesh_data_size });

    upload_commands.CopyBuffer(upload_buffer, 0, mesh_buffer, 0, mesh_data_size);

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        mesh_buffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_GENERIC_READ);

    upload_commands.SetResourceBarriers(1, &barrier);

    GPUMeshCreateResult out {};
    out.mesh.index_count = index_count;
    out.mesh.data_buffer = std::move(mesh_buffer);
    out.mesh.vertex_views = vertex_views;
    out.mesh.index_view = index_view;

    out.upload_buffer = std::move(upload_buffer);

    return out;
}
