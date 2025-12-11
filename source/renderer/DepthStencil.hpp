#pragma once
#include <memory>

namespace KS
{
class Device;
class Texture;

class DepthStencil
{
    friend class RenderTarget;

public:
    DepthStencil();
    DepthStencil(Device& device, std::shared_ptr<Texture> texture);
    ~DepthStencil();
    void Clear(DXCommandList& commandList);
    bool IsValid() const { return m_texture != nullptr; }
    Texture* GetTexture() const { return m_texture.get(); }

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    std::shared_ptr<Texture> m_texture = nullptr;
};

} // namespace KS
