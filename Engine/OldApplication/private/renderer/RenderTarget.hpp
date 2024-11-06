#pragma once
#include <memory>
#include <renderer/InfoStructs.hpp>

class DepthStencil;
class Texture;
class Device;
class RenderTarget
{
public:
    RenderTarget();
    ~RenderTarget();

    void AddTexture(Device& device, std::shared_ptr<Texture> texture1, std::shared_ptr<Texture> texture2, std::string name);

    void Bind(Device& device, const DepthStencil* depth) const;
    void Clear(const Device& device);
    void CopyTo(Device& device, std::shared_ptr<RenderTarget> sourceRT, int sourceRtIndex, int dstRTIndex);
    void SetCopyFrom(const Device& device, int rtIndex);
    void PrepareToPresent(Device& device);
    std::shared_ptr<Texture> GetTexture(Device& device, int index);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;

    std::shared_ptr<Texture> m_textures[2][8];
    int m_textureCount = 0;
    glm::vec2 m_size {};
    Formats m_format {};
};
