#pragma once
#include <containers/ByteBuffer.hpp>
#include <optional>
#include <renderer/InfoStructs.hpp>
#include <string>

namespace KS
{

// Format: RGBA8
// Can be expanded to handle more formats
class Image
{
public:
    Image() = default;

    Image(ByteBuffer&& data, uint32_t width, uint32_t height, std::string name, Formats format)
        : width(width), height(height), data(std::move(data)), m_name(name), m_format(format)
    {
        ASSERT(this->data.GetView<uint8_t>().count() == width * height * 4 ||
               this->data.GetView<uint8_t>().count() == width * height * 4 * sizeof(float) &&
               "RGBA is currently only supported and amount of data supplied mismatches the width and height provided");
    }

    uint32_t GetWidth() const { return width; }
    uint32_t GetHeight() const { return height; }
    const ByteBuffer& GetData() const { return data; }
    std::string GetName() const { return m_name; }
    Formats GetFormat() const { return m_format; }

    static uint32_t BytesPerPixelFromFormat(Formats format);

private:
    uint32_t width{}, height{};
    ByteBuffer data{};
    std::string m_name;
    Formats m_format{};

};

std::optional<Image> LoadImageFileFromMemory(const void* filedata, size_t byte_length, std::string name, Formats format);
std::optional<ByteBuffer> SaveImageToPNG(const Image& image);

}  // namespace KS

CEREAL_CLASS_VERSION(KS::Image, 0);
