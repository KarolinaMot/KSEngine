#pragma once

#include "SubRenderer.hpp"
#include <fileio/ResourceHandle.hpp>

#include <resources/Image.hpp>
#include <resources/Mesh.hpp>
#include <resources/Model.hpp>

#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>
#include "InfoStructs.hpp"

namespace KS
{
class StorageBuffer;
class UniformBuffer;
class Device;
class Shader;
class Mesh;
class Model;
class Texture;
struct DrawEntry;

class ModelRenderer : public SubRenderer
{
public:
    ModelRenderer(const Device& device, SubRendererDesc& desc);
    ~ModelRenderer();

    void Render(Device& device, DXCommandContext* commandContext, Scene& scene,
                std::vector<std::pair<ShaderInput*, ShaderInputBindDesc>>& inputs, bool clearRT) override;

private:

    void DrawMesh(Device& device, Scene& scene, DXCommandList& commandList, int index);

    bool m_raytraced = false;
    int32_t m_frameCount = 0;
};
} // namespace KS
