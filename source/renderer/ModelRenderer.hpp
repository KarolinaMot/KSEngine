#pragma once

#include "SubRenderer.hpp"
#include <fileio/ResourceHandle.hpp>

#include <resources/Image.hpp>
#include <resources/Mesh.hpp>
#include <resources/Model.hpp>

#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>

namespace KS
{
class Buffer;

struct ModelMat
{
    glm::mat4 mModel;
    glm::mat4 mTransposed;
};

class Device;
class Shader;
class Mesh;
class Model;
class Texture;

class ModelRenderer : public SubRenderer
{
public:
    ModelRenderer(const Device& device, std::shared_ptr<Shader> shader);
    ~ModelRenderer();

    void QueueModel(ResourceHandle<Model> model, const glm::mat4& transform);
    void Render(Device& device, int cpuFrameIndex) override;

private:
    const Mesh* GetMesh(const Device& device, ResourceHandle<Mesh> mesh);
    const Model* GetModel(ResourceHandle<Model> model);
    std::shared_ptr<Texture> GetTexture(const Device& device, ResourceHandle<Texture> imgPath);

    std::unordered_map<ResourceHandle<Model>, Model> model_cache {};
    std::unordered_map<ResourceHandle<Mesh>, Mesh> mesh_cache {};
    std::unordered_map<ResourceHandle<Texture>, std::shared_ptr<Texture>> tex_cache {};

    struct DrawEntry
    {
        glm::mat4 transform {};
        ResourceHandle<Mesh> mesh {};
        Material baseTex {};
    };

    std::vector<DrawEntry> draw_queue {};
    std::shared_ptr<Buffer> mModelMatBuffer;
};
} // namespace KS
