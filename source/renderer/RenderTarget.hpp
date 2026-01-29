#pragma once
#include <memory>
#include <renderer/InfoStructs.hpp>

class DXCommandList;
namespace KS
{
class DepthStencil;
class Texture;
class Device;
class RenderTarget
{
public:
    RenderTarget();
    ~RenderTarget();

    void AddTexture(Device& device, std::shared_ptr<Texture> texture1, std::shared_ptr<Texture> texture2, std::string name);
    void AddTexture(Device& device, std::shared_ptr<Texture> texture1, std::shared_ptr<Texture> texture2, std::string name, unsigned int slot1, unsigned int slot2);
    void Bind(DXCommandList& commandList, uint32_t frameIndex, const DepthStencil* depth) const;
    void Clear(DXCommandList& commandList, uint32_t frameIndex);
    void CopyTo(DXCommandList& commandList, uint32_t frameIndex, std::shared_ptr<RenderTarget> sourceRT, int sourceRtIndex,
                int dstRTIndex);
    void SetCopyFrom(DXCommandList& commandList, uint32_t frameIndex, int rtIndex);
    void PrepareToPresent(DXCommandList& commandList, uint32_t frameIndex);
    std::shared_ptr<Texture> GetTexture(uint32_t frameIndex, int index);
    uint32_t GetWidth() const;
    uint32_t GetHeight() const;


private:
    class Impl;
    std::unique_ptr<Impl> m_impl;

    std::shared_ptr<Texture> m_textures[2][8];
    int m_textureCount = 0;
    glm::vec2 m_size {};
    Formats m_format {};
};

}