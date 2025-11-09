#pragma once
#include <memory>
#include <renderer/InfoStructs.hpp>
#include <renderer/ShaderInput.hpp>

class DXCommandList;
namespace KS
{
class Device;
class Image;
class RenderTarget;
class DepthStencil;
class UploadArena;

class Texture : public ShaderInput
{
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

    Texture(const Device& device, uint32_t width, uint32_t height, int type, glm::vec4 clearColor, Formats format,
            uint32_t mipLevels=1);

    Texture(Device& device, void* resourceHeap, DXCommandList& commandList, const Image& image);

    Texture(const Device& device, void* resourceHeap, uint32_t width, uint32_t height, int type, glm::vec4 clearColor,
            Formats format, uint32_t mipLevels);

    Texture(void* resource, uint32_t width, uint32_t height, int type);

    Texture(const Device& device, void* resourceHeap, uint32_t width, uint32_t height, int flags, glm::vec4 clearColor,
            Formats format,
            int srvAllocationSlot, int uavAllocationSlot);

    ~Texture();
    virtual void Bind(const Device& device, void* resourceHeap, DXCommandList& commandList, const ShaderInputDesc& desc,
                      uint32_t mip = 0) override;
    void TransitionToRO(void* resourceHeap, DXCommandList& commandList) const;
    void TransitionToRW(void* resourceHeap, DXCommandList& commandList) const;
    inline int GetType() const { return m_flag; }
    inline Formats GetFormat() const { return m_format; }
    GenerateMipsInfo GetMipmapInfo() const;

    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    size_t GetGPUAddress(int elementIndex, int frameIndex) const override;
    uint32_t GetHandleIndex(bool readOnly) const;

   // void GenerateMipmaps(const Device& device, DXCommandList& commandList);

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
}  // namespace KS
