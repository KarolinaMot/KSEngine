#include <Log.hpp>
#include <gpu_resources/DXResourceBuilder.hpp>
#include <rendering/GPUMaterial.hpp>
#include <resources/Image.hpp>

GPUMaterialUtility::GPUMaterialCreateResult GPUMaterialUtility::CreateGPUMaterial(DXCommandList& upload_commands, ID3D12Device* device, const Material& material)
{
    using namespace MaterialConstants;
    std::array<std::string_view, TEXTURE_COUNT> image_paths {};

    // Get all paths

    for (size_t i = 0; i < TEXTURE_COUNT; i++)
    {
        if (auto texture_present = material.GetParameterT(TextureNames[i]))
        {
            image_paths.at(i) = texture_present.value();
        }
        else
        {
            Log("GPUMaterial Creation Error: {} is not present in input Material", TextureNames[i]);
        }
    }

    // Load all textures from file

    std::array<Image, TEXTURE_COUNT> images {};

    for (size_t i = 0; i < TEXTURE_COUNT; i++)
    {
        images.at(i) = ImageUtility::LoadImageFromFile(std::string(image_paths.at(i))).value();
    }

    // Create all Texture Buffers in GPU

    std::array<DXResource, TEXTURE_COUNT> textures {};

    DXResourceBuilder builder {};
    builder
        .WithHeapType(D3D12_HEAP_TYPE_DEFAULT)
        .WithInitialState(D3D12_RESOURCE_STATE_COPY_DEST);

    for (size_t i = 0; i < TEXTURE_COUNT; i++)
    {
        textures.at(i) = builder.MakeTexture2D(device, images.at(i).GetDimensions(), DXGI_FORMAT_R8G8B8A8_UNORM).value();
    }

    // Collect all Texture alignment and memory requirements

    struct SubResourceLayoutInfo
    {
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint {};
        uint64_t row_stride {};
        uint32_t rows {};
    };

    // Layout + Offset
    std::array<std::pair<SubResourceLayoutInfo, uint64_t>, TEXTURE_COUNT> memory_layouts {};

    uint64_t upload_buffer_size = 0;

    for (size_t i = 0; i < TEXTURE_COUNT; i++)
    {
        auto desc = textures.at(i).GetInfo();
        uint64_t texture_size = 0;

        SubResourceLayoutInfo info {};

        device->GetCopyableFootprints(
            &desc,
            0, // Start subresource
            1, // Number of subresources
            upload_buffer_size, // Base offset in the upload buffer
            &info.footprint, // PlacedFootprint for this texture
            &info.rows, // Number of rows in the subresource
            &info.row_stride, // Size of each row in bytes
            &texture_size); // Total size of this texture's subresource

        memory_layouts.at(i) = std::make_pair(info, upload_buffer_size);
        upload_buffer_size += texture_size; // Accumulate total buffer size
    }

    // Create upload buffer and copy to mapped memory

    builder
        .WithHeapType(D3D12_HEAP_TYPE_UPLOAD)
        .WithInitialState(D3D12_RESOURCE_STATE_COPY_SOURCE);

    DXResource upload_resource = builder.MakeBuffer(device, upload_buffer_size).value();
    auto mapped_ptr = upload_resource.Map();

    for (size_t i = 0; i < TEXTURE_COUNT; i++)
    {
        const auto& [layout_info, offset] = memory_layouts.at(i);
        const auto& image = images.at(i);

        auto image_row_stride = image.GetDimensions().x * sizeof(uint8_t) * 4;

        std::byte* dst = mapped_ptr.Get() + offset;
        for (uint32_t row = 0; row < layout_info.rows; row++)
        {
            auto* src_ptr = image.GetStorage().GetData() + row * image_row_stride;
            auto* dst_ptr = dst + row * layout_info.row_stride;

            std::memcpy(dst_ptr, src_ptr, image_row_stride);
        }
    }

    upload_resource.Unmap(std::move(mapped_ptr), D3D12_RANGE { 0, upload_buffer_size });

    // Queue copy texture calls

    for (size_t i = 0; i < TEXTURE_COUNT; i++)
    {
        D3D12_TEXTURE_COPY_LOCATION src_location = {};

        src_location.pResource = upload_resource.Get();
        src_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src_location.PlacedFootprint = memory_layouts.at(i).first.footprint;

        D3D12_TEXTURE_COPY_LOCATION dst_location = {};

        dst_location.pResource = textures.at(i).Get();
        dst_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst_location.SubresourceIndex = 0;

        upload_commands.Get()->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, nullptr);

        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            textures.at(i).Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        upload_commands.SetResourceBarriers(1, &barrier);
    }

    // Create descriptor views

    DXDescriptorHeap<SRV> texture_heap { device, MaterialConstants::TEXTURE_COUNT };

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc {};
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    D3D12_TEX2D_SRV tex2d_view {};
    tex2d_view.MipLevels = -1;
    tex2d_view.MostDetailedMip = 0;
    tex2d_view.PlaneSlice = 0;
    tex2d_view.ResourceMinLODClamp = 0.0f;

    srv_desc.Texture2D = tex2d_view;

    for (size_t i = 0; i < TEXTURE_COUNT; i++)
    {
        texture_heap.Allocate(device, SRV { srv_desc, textures.at(i).Get() }, i);
    }

    GPUMaterialCreateResult out {};
    out.material.textures = std::move(textures);
    out.material.texture_heap = std::move(texture_heap);
    out.upload_buffer = std::move(upload_resource);

    return out;
}