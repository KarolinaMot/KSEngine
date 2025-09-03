#pragma once
#include <memory>

class DXCommandList;
namespace KS
{
class Device;
class Texture;

class DepthStencil
{
    friend class RenderTarget;

public:
    DepthStencil(Device& device, DXCommandList& commandList, std::shared_ptr<Texture>& texture);
    ~DepthStencil();
    void PrepareToUse(Device& device, DXCommandList& commandList);
    void Clear(Device& device, DXCommandList& commandList);
    bool IsValid() const { return m_texture != nullptr; }

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    std::shared_ptr<Texture> m_texture = nullptr;
};

} // namespace KS
