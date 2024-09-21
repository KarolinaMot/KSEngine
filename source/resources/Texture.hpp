#pragma once
#include <memory>
#include <renderer/InfoStructs.hpp>
namespace KS
{
class Device;
class Image;
class CommandList;
class RenderTarget;
class DepthStencil;
class Texture
{
    friend CommandList;
    friend RenderTarget;
    friend DepthStencil;

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
    Texture(const Device& device, void* resource, glm::vec2 size, TextureFlags type);
    ~Texture();
    void Bind(const Device& device, uint32_t rootIndex, bool readOnly = true) const;
    inline TextureFlags GetType()const{return m_flag;}
    inline Formats GetFormat() const { return m_format;}
private:
    class Impl;
    Impl* m_impl;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    glm::vec4 m_clearColor = glm::vec4(0.f);
    Formats m_format;
    TextureFlags m_flag;
};
} // namespace KS
