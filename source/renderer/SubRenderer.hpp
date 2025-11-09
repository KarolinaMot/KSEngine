#pragma once
#include <memory>
#include <string>
#include <vector>
#include <renderer/Shader.hpp>

struct DXCommandContext;
namespace KS
{
class Device;
class RenderTarget;
class DepthStencil;
class Texture;
class Scene;
struct ShaderInputBindDesc;
class ShaderInput;

struct RenderParameters
{
    Scene* scene{};
    std::shared_ptr<RenderTarget> rt{};
    std::shared_ptr<DepthStencil> ds{};
    std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>* inputs;
    bool clearRt = true;
};

class SubRenderer
{
public:
    SubRenderer(const Device&, std::shared_ptr<Shader>& shader)
        : m_shader(shader){};
    virtual ~SubRenderer() = default;
    virtual void Render(Device& device, DXCommandContext* commandContext, RenderParameters& par) = 0;
    const Shader* GetShader() const { return m_shader.get(); }

    void Recompile(const Device& device) { m_shader->Compile(device); }

protected:
    std::shared_ptr<Shader> m_shader;
};
}  // namespace KS