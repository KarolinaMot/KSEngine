#include "Image.hpp"
#pragma warning(push, 0)
#pragma warning(disable : 4996)
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>
#pragma warning(pop)

#include <tools/Log.hpp>

std::optional<KS::Image> KS::LoadImageFileFromMemory(const void* filedata, size_t byte_length, std::string name, Formats format)
{
    int height {}, width {}, comp {};
    if (format == Formats::R32G32B32A32_FLOAT)
    {
        float* stbi_result = stbi_loadf_from_memory((const stbi_uc*)filedata, static_cast<int>(byte_length), &width, &height,
                                                    &comp, 4);  // 4 float channels

        if (stbi_result != nullptr)
        {
            // 16 bytes per pixel: 4 channels * 4 bytes
            uint32_t byteSize = static_cast<uint32_t>(width * height * Image::BytesPerPixelFromFormat(format));

            ByteBuffer image_data{reinterpret_cast<const std::byte*>(stbi_result), byteSize};

            STBI_FREE(stbi_result);

            return Image{std::move(image_data), static_cast<uint32_t>(width), static_cast<uint32_t>(height), name, format};
        }
    }
    else
    {
        stbi_uc* stbi_result = stbi_load_from_memory((const stbi_uc*)filedata, static_cast<int>(byte_length), &width, &height,
                                                     &comp, 4);

        if (stbi_result != nullptr)
        {
            uint32_t byteSize = static_cast<uint32_t>(width * height * Image::BytesPerPixelFromFormat(format));  // 4 bytes per pixel

            ByteBuffer image_data{reinterpret_cast<const std::byte*>(stbi_result), byteSize};

            STBI_FREE(stbi_result);

            return Image{std::move(image_data), static_cast<uint32_t>(width), static_cast<uint32_t>(height), name, format};
        }
    }

    LOG(Log::Severity::WARN, "Failure with STBI load: {}", stbi_failure_reason());
    return std::nullopt;
}

std::optional<KS::ByteBuffer> KS::SaveImageToPNG(const Image& image)
{
    const auto* image_data = image.GetData().GetView<unsigned char>().begin();

    int out_length {};
    auto* stbi_result = stbi_write_png_to_mem(image_data, image.GetWidth() * 4, image.GetWidth(), image.GetHeight(), 4, &out_length);

    if (stbi_result)
    {
        ByteBuffer compressed_data { stbi_result, static_cast<size_t>(out_length) };
        STBI_FREE(stbi_result);
        return compressed_data;
    }
    else
    {
        LOG(Log::Severity::WARN, "Failure with STBI write: {}", stbi_failure_reason());
        return std::nullopt;
    }
}

uint32_t KS::Image::BytesPerPixelFromFormat(Formats format) 
{ 
    switch (format)
    {
        case Formats::R8G8B8A8_UNORM_SRGB:
        case Formats::R8G8B8A8_UNORM:
            return 4;
        case Formats::R16G16B16A16_FLOAT:
            return 8;

        case Formats::R32G32B32A32_FLOAT:
            return 16;

        default:
            assert(false && "Unknown format in BytesPerPixelFromFormat");
            return 4;
    }
}
