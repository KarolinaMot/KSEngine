#pragma once
#include <memory>

class Device;
class Texture;

class DepthStencil
{
    friend class RenderTarget;

public:
    DepthStencil(Device& device, std::shared_ptr<Texture> texture);
    ~DepthStencil();
    void Clear(Device& device);
    bool IsValid() const { return m_texture != nullptr; }

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    std::shared_ptr<Texture> m_texture = nullptr;
};
