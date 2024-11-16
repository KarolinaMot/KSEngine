#include <resources/Image.hpp>

#include <Log.hpp>
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

std::optional<Image> LoadImageFileFromMemory(const void* filedata, size_t byte_length)
{
    int height {}, width {}, comp {};
    auto* stbi_result = stbi_load_from_memory((const stbi_uc*)(filedata), byte_length, &width, &height, &comp, 4);

    if (stbi_result != nullptr)
    {
        ByteBuffer image_data { stbi_result, static_cast<uint32_t>(width * height * 4) };
        std::free(stbi_result);
        return Image { std::move(image_data), static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
    }
    else
    {
        Log("Failure with STBI load: {}", stbi_failure_reason());
        return std::nullopt;
    }
}

std::optional<ByteBuffer> SaveImageToPNG(const Image& image)
{
    const auto* image_data = image.GetStorage().GetData();

    auto write_to_byte_buffer = [](void* context, void* data, int size)
    {
        auto& out_byte_buffer = *static_cast<ByteBuffer*>(context);
        out_byte_buffer = { data, static_cast<size_t>(size) };
    };

    ByteBuffer output {};
    bool result = static_cast<bool>(stbi_write_png_to_func(
        write_to_byte_buffer, &output,
        image.GetWidth(), image.GetHeight(), 4,
        image_data, image.GetWidth() * 4));

    if (result)
    {
        return output;
    }
    else
    {
        Log("Failure with STBI write: {}", stbi_failure_reason());
        return std::nullopt;
    }
}
