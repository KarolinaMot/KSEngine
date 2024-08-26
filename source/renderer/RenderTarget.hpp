#pragma once
#include <memory>
#include <renderer/InfoStructs.hpp>

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

    void AddTexture(Device& device, std::shared_ptr<Texture>& texture);

    void Bind(Device& device, const DepthStencil* depth) const;
    void Clear(const Device& device);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;

    std::shared_ptr<Texture> m_textures[8];
    int m_textureCount = 0;
    glm::vec2 m_size {};
    glm::vec4 m_clearColor {};
    Formats m_format {};
};

}