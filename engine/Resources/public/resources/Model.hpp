#pragma once
#include <Common.hpp>
#include <SerializationCommon.hpp>

#include <assimp/postprocess.h>
#include <glm/mat4x4.hpp>
#include <resources/Material.hpp>
#include <resources/Mesh.hpp>

// Imported models are flat, without a transform hierarchy
class Model
{
public:
    struct Node
    {
        glm::mat4 transform {};
        std::vector<std::pair<size_t, size_t>> mesh_material_indices {};

        void save(JSONSaver& a) const;
        void load(JSONLoader& a);
    };

    std::vector<Node> nodes;
    std::vector<std::string> meshes;
    std::vector<Material> materials;

private:
    friend class cereal::access;
    void save(JSONSaver& ar, const uint32_t v) const;
    void load(JSONLoader& ar, const uint32_t v);
};

namespace ModelUtility
{
constexpr uint32_t DEFAULT_POST_PROCESSING_FLAGS = aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_EmbedTextures | aiProcess_FlipUVs;

// Converts a model file into a .json file for the engine to use
// Return value is the newly imported model file
std::optional<Model> ImportFromFile(const std::string& source_model, uint32_t post_process_flags = DEFAULT_POST_PROCESSING_FLAGS);

std::optional<Model> LoadFromFile(const std::string& path);
}

CEREAL_CLASS_VERSION(Model, 0);
