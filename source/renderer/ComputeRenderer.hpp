#pragma once
#include "SubRenderer.hpp"

namespace KS
{
class Scene;
class ComputeRenderer : public SubRenderer
{
public:
    ComputeRenderer(const Device& device, SubRendererDesc& desc);
    ~ComputeRenderer();

    void Render(Device& device, int commandListID, Scene& scene,
                std::vector<std::pair<ShaderInput*, ShaderInputDesc>>& inputs, bool clearRT) override;

private:
};
}  // namespace KS
