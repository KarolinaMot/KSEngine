#pragma once
#include <memory>
#include <renderer/InfoStructs.hpp>

namespace KS
{
class Device;
class Image;
class Texture
{
    friend class RenderTarget;
    friend class DepthStencil;

public:
    enum TextureFlags
    {
        RO_TEXTURE,
        RW_TEXTURE,
        RENDER_TARGET,
        DEPTH_TEXTURE
    };

    Texture(const Device& device, const Image& image, TextureFlags type = RO_TEXTURE);
    Texture(const Device& device, uint32_t width, uint32_t height, TextureFlags type, glm::vec4 clearColor, Formats format);
    ~Texture();
    void Bind(const Device& device, uint32_t rootIndex, bool readOnly = true) const;

    Formats GetFormat() const { return m_format; }
    TextureFlags GetType() const { return m_flag; }

private:
    class Impl;
    Impl* m_impl;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    Formats m_format;
    TextureFlags m_flag;
};
} // namespace KS
