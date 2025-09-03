#pragma once

#include "SubRenderer.hpp"

class DXCommandList;
namespace KS
{
class Device;
class Scene;
class ModelRenderer : public SubRenderer
{
public:
    ModelRenderer(const Device& device, SubRendererDesc& desc);

    ~ModelRenderer();

    void Render(Device& device, int commandListID, Scene& scene, std::vector<std::pair<ShaderInput*, ShaderInputDesc>>& inputs,
                bool clearRT) override;

private:
    void DrawMesh(Device& device, Scene& scene, DXCommandList& commandList, int index);
};
}  // namespace KS

