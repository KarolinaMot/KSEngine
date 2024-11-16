#pragma once
#include <Common.hpp>

#include <cassert>
#include <cereal/cereal.hpp>
#include <optional>
#include <resources/ByteBuffer.hpp>

// Format: RGBA8
// Can be expanded to handle more formats
class Image
{
public:
    Image() = default;

    Image(ByteBuffer&& data, uint32_t width, uint32_t height)
        : width(width)
        , height(height)
        , data(std::move(data))
    {
        assert(this->data.GetView<uint8_t>().count() == width * height * 4
            && "RGBA is currently only supported and amount of data supplied mismatches the width and height provided");
    }

    uint32_t GetWidth() const { return width; }
    uint32_t GetHeight() const { return height; }
    const ByteBuffer& GetStorage() const { return data; }

private:
    uint32_t width {}, height {};
    ByteBuffer data {};
};

std::optional<Image> LoadImageFileFromMemory(const void* filedata, size_t byte_length);
std::optional<ByteBuffer> SaveImageToPNG(const Image& image);

CEREAL_CLASS_VERSION(Image, 0);