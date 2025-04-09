#pragma once
#include <memory>
#include <renderer/InfoStructs.hpp>
#include <renderer/ShaderInput.hpp>

namespace KS
{
class Device;
class Image;
class CommandList;
class RenderTarget;
class DepthStencil;
class Texture : public ShaderInput
{
    friend CommandList;
    friend RenderTarget;
    friend DepthStencil;

public:
    enum TextureFlags
    {
        RO_TEXTURE = 1 << 0,
        RW_TEXTURE = 1 << 1,
        RENDER_TARGET = 1 << 2,
        DEPTH_TEXTURE = 1 << 3
    };

    struct GenerateMipsInfo
    {
        uint32_t SrcMipLevel;   // Texture level of source mip
        uint32_t NumMipLevels;  // Number of OutMips to write: [1-4]
        uint32_t SrcDimension;  // Width and height of the source texture are even or odd.
        uint32_t IsSRGB;        // Must apply gamma correction to sRGB textures.
        glm::vec2 TexelSize;    // 1.0 / OutMip1.Dimensions
    };


    Texture(Device& device, const Image& image, int flags = 0);
    Texture(const Device& device, uint32_t width, uint32_t height, int flags, glm::vec4 clearColor, Formats format, int mipLevels = 1);
    Texture(const Device& device, void* resource, glm::vec2 size, int flags = 0);
    Texture(const Device& device, uint32_t width, uint32_t height, int flags, glm::vec4 clearColor, Formats format,
        int srvAllocationSlot, int uavAllocationSlot);
    ~Texture();
    void Bind(Device& device, const ShaderInputDesc& desc, uint32_t offsetIndex = 0);
    void TransitionToRO(const Device& device) const;
    void TransitionToRW(const Device& device) const;
    inline int GetType() const { return m_flag; }
    inline Formats GetFormat() const { return m_format;}

    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }

    void GenerateMipmaps(Device& device);


private:

    class Impl;
    Impl* m_impl;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_mipLevels = 1;

    glm::vec4 m_clearColor = glm::vec4(0.f);
    Formats m_format;
    int m_flag;
};
} // namespace KS
