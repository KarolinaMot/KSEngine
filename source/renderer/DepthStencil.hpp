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
    DepthStencil(Device& device, std::shared_ptr<Texture> texture);
    ~DepthStencil();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    std::shared_ptr<Texture> m_texture;
};

} // namespace KS
