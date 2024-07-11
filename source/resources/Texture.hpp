#pragma once
#include <memory>
namespace KS
{
class Device;
class Image;
class Texture
{
public:
    Texture(const Device& device, const Image& image, bool readOnly = true);
    Texture(const Device& device, uint32_t width, uint32_t height, bool readOnly = true);
    ~Texture();

    void BindToGraphics(const Device& device, uint32_t rootIndex, bool readOnly = true) const;
    void BindToCompute(const Device& device, uint32_t rootIndex, bool readOnly = true) const;

private:
    class Impl;
    Impl* m_impl;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};
} // namespace KS
