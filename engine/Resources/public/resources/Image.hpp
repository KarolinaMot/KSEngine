#pragma once
#include <Common.hpp>

#include <cassert>
#include <optional>
#include <resources/ByteBuffer.hpp>

// Format: RGBA8
class Image
{
public:
    Image() = default;

    Image(ByteBuffer&& data, uint32_t width, uint32_t height)
        : width(width)
        , height(height)
        , data(std::move(data))
    {
        assert(this->data.GetSize() == width * height * 4 && "Input data is not correctly formatted");
    }

    uint32_t GetWidth() const { return width; }
    uint32_t GetHeight() const { return height; }
    const ByteBuffer& GetStorage() const { return data; }

private:
    uint32_t width {}, height {};
    ByteBuffer data {};
};

namespace ImageUtility
{
std::optional<Image> LoadImageFromFile(const std::string& path);
std::optional<Image> LoadImageFromData(const void* data, size_t size);

std::optional<ByteBuffer> SaveImageToPNGBuffer(const Image& image);
bool SaveImageToPNGFile(const Image& image, const std::string& file);
}