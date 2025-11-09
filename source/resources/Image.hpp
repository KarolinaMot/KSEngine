#pragma once
#include <containers/ByteBuffer.hpp>
#include <optional>

namespace KS
{

// Format: RGBA8
// Can be expanded to handle more formats
class Image
{
public:
    Image() = default;

    Image(ByteBuffer&& data, uint32_t width, uint32_t height, std::string name)
        : width(width)
        , height(height)
        , data(std::move(data)), m_name(name)
    {
        ASSERT(this->data.GetView<uint8_t>().count() == width * height * 4
            && "RGBA is currently only supported and amount of data supplied mismatches the width and height provided");
    }

    uint32_t GetWidth() const { return width; }
    uint32_t GetHeight() const { return height; }
    const ByteBuffer& GetData() const { return data; }
    std::string GetName() const { return m_name; }

    private:
    uint32_t width {}, height {};
    ByteBuffer data {};
    std::string m_name;
};

std::optional<Image> LoadImageFileFromMemory(const void* filedata, size_t byte_length, std::string name);
std::optional<ByteBuffer> SaveImageToPNG(const Image& image);
}

CEREAL_CLASS_VERSION(KS::Image, 0);