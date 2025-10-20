#pragma once
#include <memory>
#include <renderer/InfoStructs.hpp>
#include <renderer/ShaderInput.hpp>

class DXCommandList;
namespace KS
{
class Device;
class Texture;
class RenderTarget;
class DepthStencil;
class UploadArena;

class Skydome : public ShaderInput
{
    friend RenderTarget;
    friend DepthStencil;

public:
    Skydome(Device& device, DXCommandList& commandList, const Texture& image);
    virtual void Bind(const Device& device, DXCommandList& commandList, const ShaderInputDesc& desc, uint32_t offsetIndex = 0);
    void BindForCreation(const Device& device, DXCommandList& commandList, const ShaderInputDesc& desc,
                      uint32_t offsetIndex = 0);

    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    size_t GetGPUAddress(int elementIndex, int frameIndex) const override;
    uint32_t GetHandleIndex(bool readOnly) const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
};
}  // namespace KS
