#include "Model.hpp"

#include "Image.hpp"
#include "Mesh.hpp"
#include <FileIO.hpp>
#include <Log.hpp>
#include <assimp/GltfMaterial.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <filesystem>
#include <glm/gtc/type_ptr.hpp>

namespace detail
{

MeshData ProcessMesh(const aiMesh* mesh)
{
    using namespace MeshConstants;

    // Build Mesh
    MeshData new_mesh {};

    // Indices
    if (mesh->HasFaces())
    {
        std::vector<uint32_t> indices;

        for (size_t i = 0; i < mesh->mNumFaces; i++)
        {
            auto& face = mesh->mFaces[i];
            if (face.mNumIndices == 3)
            {
                indices.push_back(face.mIndices[0]);
                indices.push_back(face.mIndices[1]);
                indices.push_back(face.mIndices[2]);
            }
        }

        auto buffer = ByteBuffer(indices.data(), indices.size());
        new_mesh.AddAttribute(ATTRIBUTE_INDICES_NAME, std::move(buffer));
    }

    // Positions
    if (mesh->HasPositions())
    {
        auto buffer = ByteBuffer(mesh->mVertices, mesh->mNumVertices);
        new_mesh.AddAttribute(ATTRIBUTE_POSITIONS_NAME, std::move(buffer));
    }

    // Normals
    if (mesh->HasNormals())
    {
        auto buffer = ByteBuffer(mesh->mNormals, mesh->mNumVertices);
        new_mesh.AddAttribute(ATTRIBUTE_NORMALS_NAME, std::move(buffer));
    }

    // Tangents and Bitangents
    if (mesh->HasTangentsAndBitangents())
    {
        auto buffer = ByteBuffer(mesh->mTangents, mesh->mNumVertices);
        new_mesh.AddAttribute(ATTRIBUTE_TANGENTS_NAME, std::move(buffer));

        auto buffer2 = ByteBuffer(mesh->mBitangents, mesh->mNumVertices);
        new_mesh.AddAttribute(ATTRIBUTE_BITANGENTS_NAME, std::move(buffer2));
    }

    // Texture UVS (only using the first)
    if (mesh->GetNumUVChannels())
    {
        std::vector<glm::vec2> texture_uvs {};
        for (size_t i = 0; i < mesh->mNumVertices; i++)
        {
            texture_uvs.emplace_back(
                mesh->mTextureCoords[0][i].x,
                mesh->mTextureCoords[0][i].y);
        }

        auto buffer = ByteBuffer(texture_uvs.data(), texture_uvs.size());
        new_mesh.AddAttribute(ATTRIBUTE_TEXTURE_UVS_NAME, std::move(buffer));
    }

    return new_mesh;
}

Image ProcessImage(const aiTexture* texture)
{
    if (texture->mHeight == 0)
    {
        if (auto image_load = LoadImageFileFromMemory(texture->pcData, texture->mWidth))
        {
            return image_load.value();
        }
        else
        {
            Log("Failed to import model texture {}", texture->mFilename.C_Str());
            return Image {};
        }
    }
    else
    {
        std::vector<aiTexel> reordered_data;
        reordered_data.resize(texture->mWidth * texture->mHeight);

        for (size_t i = 0; i < reordered_data.size(); i++)
        {
            auto& texel = texture->pcData[i];
            reordered_data.at(i) = { texel.r, texel.g, texel.b, texel.a };
        }

        return Image { ByteBuffer(reordered_data.data(), reordered_data.size()), texture->mWidth, texture->mHeight };
    }
}

void ProcessNodesRecursive(std::vector<Model::Node>& out, const aiScene* scene, const aiNode* target_node, const glm::mat4& parent_transform)
{
    glm::mat4 transform = parent_transform * glm::make_mat4(&target_node->mTransformation.a1);

    std::vector<std::pair<size_t, size_t>> mm {};
    for (size_t i = 0; i < target_node->mNumMeshes; i++)
    {
        auto mesh_index = target_node->mMeshes[i];
        auto material_index = scene->mMeshes[mesh_index]->mMaterialIndex;
        mm.emplace_back(mesh_index, material_index);
    }

    out.emplace_back(transform, mm);

    for (size_t i = 0; i < target_node->mNumChildren; i++)
    {
        ProcessNodesRecursive(out, scene, target_node->mChildren[i], transform);
    }
}

Material ProcessMaterial(const std::vector<std::string>& image_paths, const aiMaterial* material)
{
    using namespace MaterialConstants;
    Material out;

    // Base colour factor
    if (aiColor4D t {}; material->Get(AI_MATKEY_BASE_COLOR, t) == aiReturn_SUCCESS)
    {
        out.AddParameter(BASE_COLOUR_FACTOR_NAME, glm::vec4 { t.r, t.g, t.b, t.a });
    }
    else
    {
        out.AddParameter(BASE_COLOUR_FACTOR_NAME, glm::vec4 { t.r, t.g, t.b, t.a });
    }

    // Occlusion Roughness Metallic
    {
        glm::vec4 orm = ORM_FACTORS_DEFAULT;
        material->Get(AI_MATKEY_GLTF_TEXTURE_STRENGTH(aiTextureType_AMBIENT_OCCLUSION, 0), orm.x);
        material->Get(AI_MATKEY_ROUGHNESS_FACTOR, orm.y);
        material->Get(AI_MATKEY_METALLIC_FACTOR, orm.z);
        out.AddParameter(ORM_FACTORS_NAME, orm);
    }

    // Normal Emissive Alpha Cutoff
    {
        glm::vec4 nea = NEA_FACTORS_DEFAULT;
        material->Get(AI_MATKEY_GLTF_TEXTURE_SCALE(aiTextureType_NORMALS, 0), nea.x);
        material->Get(AI_MATKEY_EMISSIVE_INTENSITY, nea.y);
        material->Get(AI_MATKEY_GLTF_ALPHACUTOFF, nea.z);
        out.AddParameter(NEA_FACTORS_NAME, nea);
    }

    // Double Sided
    {
        int d = DOUBLE_SIDED_DEFAULT;
        material->Get(AI_MATKEY_TWOSIDED, d);
        out.AddParameter(DOUBLE_SIDED_FLAG_NAME, static_cast<bool>(d));
    }

    auto GetTexture = [&](aiTextureType type) -> std::optional<std::string>
    {
        aiString texture_name {};
        if (material->GetTexture(type, 0, &texture_name) != aiReturn_SUCCESS)
            return {};

        std::string name = std::string(texture_name.C_Str());
        if (name.front() == '*')
        {
            int index = std::atoi(&name[1]);
            return image_paths.at(index);
        }
        else
        {
            Log("Support for non embedded textures not implemented");
            return {};
        }
    };

    if (auto path = GetTexture(aiTextureType_BASE_COLOR))
    {
        out.AddParameter(BASE_TEXTURE_NAME, ResourceHandle<Texture> { path.value() });
    }

    if (auto path = GetTexture(aiTextureType_NORMALS))
    {
        out.AddParameter(NORMAL_TEXTURE_NAME, ResourceHandle<Texture> { path.value() });
    }

    if (auto path = GetTexture(aiTextureType_LIGHTMAP))
    {
        out.AddParameter(OCCLUSION_TEXTURE_NAME, ResourceHandle<Texture> { path.value() });
    }

    if (auto path = GetTexture(aiTextureType_METALNESS))
    {
        out.AddParameter(METALLIC_TEXTURE_NAME, ResourceHandle<Texture> { path.value() });
    }

    if (auto path = GetTexture(aiTextureType_EMISSIVE))
    {
        out.AddParameter(EMISSIVE_TEXTURE_NAME, ResourceHandle<Texture> { path.value() });
    }

    return out;
}
}

std::optional<ResourceHandle<Model>> ModelImporter::ImportFromFile(const FileIO::Path& source_model, uint32_t post_processing_flags)
{
    std::filesystem::path source_model_path = source_model;

    Assimp::Importer importer;
    const aiScene* scene = nullptr;

    // Read Scene File
    {
        if (auto file_data = FileIO::OpenReadStream(source_model_path.string(), std::ios::binary))
        {
            auto dump = FileIO::DumpFullStream(file_data.value());
            scene = importer.ReadFileFromMemory(dump.data(), dump.size(), post_processing_flags);
        }
        else
        {
            Log("Could not open file: {}", source_model_path.string());
            return {};
        }

        if (scene == nullptr)
        {
            Log("Could not import model: {}", importer.GetErrorString());
            return {};
        }
    }

    // Validation
    if (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)
    {
        Log("Imported Model is incomplete");
    }

    auto source = source_model_path;

    auto base_dir = source.make_preferred().parent_path();
    auto out_dir = base_dir / source.stem();

    FileIO::MakeDirectory(out_dir.string());

    std::vector<ResourceHandle<MeshData>> mesh_paths;

    // Process all meshes
    {
        auto mesh_out = out_dir / "meshes";
        FileIO::MakeDirectory(mesh_out.string());

        for (size_t i = 0; i < scene->mNumMeshes; i++)
        {
            auto mesh = detail::ProcessMesh(scene->mMeshes[i]);
            std::string mesh_name {};

            if (scene->mMeshes[i]->mName.length == 0)
            {
                mesh_name = "mesh" + std::to_string(i);
            }
            else
            {
                mesh_name = scene->mMeshes[i]->mName.C_Str();
            }

            auto output_path = (mesh_out / (mesh_name + ".bin")).string();

            if (auto out = FileIO::OpenWriteStream(output_path))
            {
                BinarySaver ar { out.value() };
                ar(mesh);
            }
            else
            {
                Log("Failed to write output mesh file {}", output_path);
            }

            mesh_paths.emplace_back(output_path);
        }
    }

    std::vector<std::string> image_paths;

    // Process all Images
    {

        auto images_out = out_dir / "textures";
        FileIO::MakeDirectory(images_out.string());

        for (size_t i = 0; i < scene->mNumTextures; i++)
        {
            auto ai_image = scene->mTextures[i];
            auto image = detail::ProcessImage(ai_image);

            std::string image_name {};

            if (scene->mTextures[i]->mFilename.length == 0)
            {
                image_name = "texture" + std::to_string(i);
            }
            else
            {
                image_name = scene->mTextures[i]->mFilename.C_Str();
            }

            auto output_path = (images_out / (image_name + ".png")).string();

            auto output_file = FileIO::OpenWriteStream(output_path);
            auto compressed_data = SaveImageToPNG(image);

            if (output_file && compressed_data)
            {
                auto ptr = compressed_data.value().GetView<char>().begin();
                auto size = compressed_data.value().GetView<char>().count();

                output_file.value()
                    .write(ptr, size);
            }
            else
            {
                Log("Failed to write output texture file {}", output_path);
            }

            image_paths.emplace_back(output_path);
        }
    }

    std::vector<Material> materials;

    // Process Materials
    {
        for (size_t i = 0; i < scene->mNumMaterials; i++)
        {
            auto m = scene->mMaterials[i];
            auto material = detail::ProcessMaterial(image_paths, m);

            materials.emplace_back(material);
        }
    }

    std::vector<Model::Node> nodes;

    // Process Nodes
    {
        detail::ProcessNodesRecursive(nodes, scene, scene->mRootNode, glm::identity<glm::mat4>());
    }

    auto out_model_file = out_dir / (source.filename().replace_extension().string() + ".json");

    if (auto out = FileIO::OpenWriteStream(out_model_file.string(), std::ios::trunc))
    {
        JSONSaver json { out.value() };

        Model imported {
            .nodes = std::move(nodes),
            .meshes = std::move(mesh_paths),
            .materials = std::move(materials)
        };

        json(imported);
        Log("Successfully imported model from {}", source_model_path.string());
        return ResourceHandle<Model> { out_model_file.string() };
    }
    else
    {
        Log("Failed to create output model file {}", out_model_file.string());
        return std::nullopt;
    }

    return ResourceHandle<Model> { out_model_file.string() };
}