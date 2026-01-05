#pragma once

#include "SubRenderer.hpp"
#include <fileio/ResourceHandle.hpp>

#include <resources/Image.hpp>
#include <resources/Mesh.hpp>
#include <resources/Model.hpp>

#pragma warning(push, 0)
#include <glm/glm.hpp>
#pragma warning(pop)
#include <memory>
#include <unordered_map>
#include "InfoStructs.hpp"

class DXDescHeap;

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
    ModelRenderer(const Device& device, std::shared_ptr<Shader>& shader, bool onlyCubemap = false);
    ~ModelRenderer();

    void Render(Device& device, DXCommandContext* commandContext, RenderParameters& par) override;

private:

    void DrawMesh(const Device& device, DXCommandList& commandList, const BatchRange& batch,
                  const ShaderInputDesc& modelIndexInputDesc, DXDescHeap* resourceHeap, UniformBuffer* modelIndexUBO,
                  int shaderFlags, uint32_t texturesRootIndex);

    bool m_raytraced = false;
    int32_t m_frameCount = 0;
    bool m_onlyCubemap = false;
};
} // namespace KS
