#include <gpu_resources/DXResourceBuilder.hpp>
#include <rendering/GPUMaterial.hpp>
#include <resources/Image.hpp>

GPUMaterialUtility::GPUMaterialCreateResult GPUMaterialUtility::CreateGPUMaterial(DXCommandList& upload_commands, ID3D12Device* device, const Material& material)
{
    auto base_colour_path = material.GetParameterT(MaterialConstants::TextureNames[MaterialConstants::BASE_TEXTURE]).value();
    Image base_colour_image = ImageUtility::LoadImageFromFile(std::string(base_colour_path)).value();

    DXResourceBuilder builder {};
    builder
        .WithHeapType(D3D12_HEAP_TYPE_UPLOAD)
        .WithInitialState(D3D12_RESOURCE_STATE_COPY_SOURCE);

    size_t total_image_size = base_colour_image.GetStorage().GetSize();
    auto upload_buffer = builder.MakeBuffer(device, total_image_size).value();

    auto mapped_ptr = upload_buffer.Map(0);
    std::memcpy(mapped_ptr.Get(), base_colour_image.GetStorage().GetData(), total_image_size);
    upload_buffer.Unmap(std::move(mapped_ptr), D3D12_RANGE { 0, total_image_size });

    builder
        .WithHeapType(D3D12_HEAP_TYPE_DEFAULT)
        .WithInitialState(D3D12_RESOURCE_STATE_COPY_DEST);

    auto texture_resource = builder.MakeTexture2D(device, base_colour_image.GetDimensions(), DXGI_FORMAT_R8G8B8A8_UNORM).value();

    DXDescriptorHeap<SRV> texture_heap { device, 1 };

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc {};
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    // texture_heap.Allocate(device, SRV{}, 0);

    GPUMaterialCreateResult out {};
    out.material.base_texture = std::move(texture_resource);
    out.material.texture_heap = std::move(texture_heap);
    out.upload_buffer = std::move(upload_buffer);

    return out;
}