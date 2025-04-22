#pragma once

#include "SubRenderer.hpp"

namespace KS
{
class Device;
class Scene;

class ModelRenderer : public SubRenderer
{
public:
    ModelRenderer(const Device& device, SubRendererDesc& desc);

    ~ModelRenderer();

    void Render(Device& device, Scene& scene, std::vector<std::pair<ShaderInput*, ShaderInputDesc>>& inputs,
                bool clearRT = true) override;
};
}  // namespace KS

