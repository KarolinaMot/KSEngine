#include <resources/Image.hpp>

#include <FileIO.hpp>
#include <Log.hpp>
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

namespace ImageUtility
{
std::optional<Image> LoadImageFromFile(const std::string& path);
std::optional<Image> LoadImageFromData(const void* data, size_t size);

std::optional<ByteBuffer> SaveImageToPNGBuffer(const Image& image);
bool SaveImageToPNGFile(const Image& image, const std::string& file);
}

std::optional<Image> ImageUtility::LoadImageFromData(const void* filedata, size_t byte_length)
{
    int height {}, width {}, comp {};

    auto* stbi_result = stbi_load_from_memory(
        static_cast<const stbi_uc*>(filedata),
        byte_length,
        &width,
        &height,
        &comp,
        4);

    if (stbi_result != nullptr)
    {
        ByteBuffer image_data { stbi_result, static_cast<uint32_t>(width * height * 4) };
        std::free(stbi_result);
        return Image { std::move(image_data), static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
    }
    else
    {
        Log("Image Loading Error: STBI {}", stbi_failure_reason());
        return std::nullopt;
    }
}

std::optional<Image> ImageUtility::LoadImageFromFile(const std::string& path)
{
    if (auto stream = FileIO::OpenReadStream(path))
    {
        auto data = FileIO::DumpFullStream(stream.value());
        return LoadImageFromData(data.data(), data.size());
    }

    Log("Image Loading Error: Could not read file {}", path);
    return std::nullopt;
}

std::optional<ByteBuffer> ImageUtility::SaveImageToPNGBuffer(const Image& image)
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

    Log("Image Saving Error: STBI {}", stbi_failure_reason());
    return std::nullopt;
}

bool ImageUtility::SaveImageToPNGFile(const Image& image, const std::string& file)
{
    auto write_flags = std::ios::out | std::ios::trunc | std::ios::binary;
    auto stream = FileIO::OpenWriteStream(file, write_flags);

    if (!stream)
    {
        Log("Image Saving Error: Could not write to file {}", file);
        return false;
    }

    auto bytebuffer = ImageUtility::SaveImageToPNGBuffer(image);

    if (!bytebuffer)
    {
        return false;
    }

    stream->write(bytebuffer->GetView<char>().begin(), bytebuffer->GetView<char>().count());
    return true;
}
