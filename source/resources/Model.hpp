#pragma once
#include "Material.hpp"
#include <assimp/postprocess.h>
#include <fileio/FileIO.hpp>
#include <fileio/Serialization.hpp>

namespace KS
{

class Mesh;

// Imported models are flat, without a transform hierarchy
class Model
{
public:
    struct Node
    {
        glm::mat4 transform {};
        std::vector<std::pair<size_t, size_t>> mesh_material_indices {};

        template <typename A>
        void serialize(A& a);
    };

    std::vector<Node> nodes;
    std::vector<ResourceHandle<Mesh>> meshes;
    std::vector<Material> materials;

private:
    friend class cereal::access;

    template <typename A>
    void save(A& ar, const uint32_t v) const;

    template <typename A>
    void load(A& ar, const uint32_t v);
};

template <typename A>
inline void Model::Node::serialize(A& ar)
{
    ar(cereal::make_nvp("Transform", transform));
    ar(cereal::make_nvp("MeshAndMaterial", mesh_material_indices));
}

template <typename A>
inline void Model::save(A& ar, const uint32_t v) const
{
    switch (v)
    {
    case 0:
        ar(cereal::make_nvp("Nodes", nodes));
        ar(cereal::make_nvp("Meshes", meshes));
        ar(cereal::make_nvp("Materials", materials));
        break;

    default:
        break;
    }
}

template <typename A>
inline void Model::load(A& ar, const uint32_t v)
{
    switch (v)
    {
    case 0:
        ar(cereal::make_nvp("Nodes", nodes));
        ar(cereal::make_nvp("Meshes", meshes));
        ar(cereal::make_nvp("Materials", materials));
        break;

    default:
        break;
    }
}

namespace ModelImporter
{

    constexpr uint32_t DEFAULT_POST_PROCESSING_FLAGS = aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_EmbedTextures | aiProcess_FlipUVs;

    // Converts a model file into a .json file for the engine to use
    // Return value is the newly imported model file
    std::optional<ResourceHandle<Model>>
    ImportFromFile(const FileIO::Path& source_model, uint32_t post_process_flags = DEFAULT_POST_PROCESSING_FLAGS);
}
}

CEREAL_CLASS_VERSION(KS::Model, 0);
