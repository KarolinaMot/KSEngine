#include "Image.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include <tools/Log.hpp>

std::optional<KS::Image> KS::LoadImageFileFromMemory(const void* filedata, size_t byte_length)
{
    int height {}, width {}, comp {};
    auto* stbi_result = stbi_load_from_memory((const stbi_uc*)(filedata), byte_length, &width, &height, &comp, 4);

    if (stbi_result != nullptr)
    {
        ByteBuffer image_data { stbi_result, static_cast<uint32_t>(width * height * 4) };
        STBI_FREE(stbi_result);
        return Image { std::move(image_data), static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
    }
    else
    {
        LOG(Log::Severity::WARN, "Failure with STBI load: {}", stbi_failure_reason());
        return std::nullopt;
    }
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
