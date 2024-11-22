#pragma once
#include <Common.hpp>

#include <cassert>
#include <glm/vec2.hpp>
#include <optional>
#include <resources/ByteBuffer.hpp>

// Format: RGBA8
class Image
{
public:
    Image() = default;

    Image(ByteBuffer&& data, glm::uvec2 dimensions)
        : dimensions(dimensions)
        , data(std::move(data))
    {
        assert(this->data.GetSize() == dimensions.x * dimensions.y * 4 && "Input data is not correctly formatted");
    }

    glm::uvec2 GetDimensions() const { return dimensions; }
    const ByteBuffer& GetStorage() const { return data; }

private:
    glm::uvec2 dimensions {};
    ByteBuffer data {};
};

namespace ImageUtility
{
std::optional<Image> LoadImageFromFile(const std::string& path);
std::optional<Image> LoadImageFromData(const void* data, size_t size);

std::optional<ByteBuffer> SaveImageToPNGBuffer(const Image& image);
bool SaveImageToPNGFile(const Image& image, const std::string& file);
}