#include "Model.hpp"

#include <assimp/GltfMaterial.h>
#include <assimp/scene.h>

#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>
#include <fileio/FileIO.hpp>
#pragma warning(push, 0)
#include <glm/gtc/type_ptr.hpp>
#pragma warning(pop)

#include <renderer/InfoStructs.hpp>
#include <tools/Log.hpp>

#include "Image.hpp"
#include "Mesh.hpp"

namespace KS
{
    MeshData Model::ProcessMesh(const aiMesh* mesh)
    {
        using namespace KS::MeshConstants;

        // Build Mesh
        MeshData new_mesh{};

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
            std::vector<glm::vec2> texture_uvs{};
            for (size_t i = 0; i < mesh->mNumVertices; i++)
            {
                texture_uvs.emplace_back(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
            }

            auto buffer = ByteBuffer(texture_uvs.data(), texture_uvs.size());
            new_mesh.AddAttribute(ATTRIBUTE_TEXTURE_UVS_NAME, std::move(buffer));
        }
        return new_mesh;
    }

    Image Model::ProcessImage(const aiTexture* texture)
    {
        if (texture->mHeight == 0)
        {
            if (auto image_load = LoadImageFileFromMemory(texture->pcData, texture->mWidth, texture->mFilename.C_Str(), Formats::R8G8B8A8_UNORM))
            {
                return image_load.value();
            }
            else
            {
                LOG(Log::Severity::WARN, "Failed to import model texture {}", texture->mFilename.C_Str());
                return Image{};
            }
        }
        else
        {
            std::vector<aiTexel> reordered_data;
            reordered_data.resize(texture->mWidth * texture->mHeight);

            for (size_t i = 0; i < reordered_data.size(); i++)
            {
                auto& texel = texture->pcData[i];
                reordered_data.at(i) = {texel.r, texel.g, texel.b, texel.a};
            }

            return Image{ByteBuffer(reordered_data.data(), reordered_data.size()), texture->mWidth, texture->mHeight,
                         texture->mFilename.C_Str(), Formats::R8G8B8A8_UNORM};
        }
    }


    void Model::ProcessLight(std::vector<DirLightInfo>& dirLights, std::vector<PointLightInfo>& pointLights,
                             const aiLight* light,
                      glm::mat4x4 transform)
    {
        if (light->mType == aiLightSource_DIRECTIONAL)
        {
            DirLightInfo out;
            glm::vec3 color = {light->mColorDiffuse.r, light->mColorDiffuse.g, light->mColorDiffuse.b};
            float intensity = glm::dot(color, glm::vec3(0.2126f, 0.7152f, 0.0722f));
            color = color / intensity;
            out.mColorAndIntensity = glm::vec4(color, intensity);
            glm::vec3 forward_negZ = glm::normalize(glm::vec3(transform[2]));
            out.mDir = {forward_negZ.x, forward_negZ.y, forward_negZ.z};
            out.mAngularRadius = 0.00465f;
            dirLights.push_back(out);
        }
        else if (light->mType == aiLightSource_POINT)
        {
            PointLightInfo out;

            out.mPosition = glm::vec4(transform[3]);
            out.mConstantAttenuation = light->mAttenuationConstant;
            out.mLinearAttenuation = light->mAttenuationLinear;
            out.mQuadraticAttenuation = light->mAttenuationQuadratic;
            glm::vec3 color = {light->mColorDiffuse.r, light->mColorDiffuse.g, light->mColorDiffuse.b};
            float intensity = glm::dot(color, glm::vec3(0.2126f, 0.7152f, 0.0722f));
            color = color / intensity;
            out.mColorAndIntensity = glm::vec4(color, intensity);
            pointLights.push_back(out);
        }
    }

    static glm::mat4 AiToGlm(const aiMatrix4x4& m)
    {
        // GLM constructor takes column-major data
        return glm::mat4(m.a1, m.b1, m.c1, m.d1, m.a2, m.b2, m.c2, m.d2, m.a3, m.b3, m.c3, m.d3, m.a4, m.b4, m.c4, m.d4);
    }

    void Model::ProcessNodesRecursive(std::vector<Model::Node>& out, std::vector<DirLightInfo>& dirLights,
                                      std::vector<uint32_t>& meshInstances,
                                      std::vector<PointLightInfo>& pointLights,
                                      const aiScene* scene,  const aiNode* target_node,
                               const glm::mat4& parent_transform)
    {
        glm::mat4 transform = parent_transform * AiToGlm(target_node->mTransformation);

        std::vector<std::pair<size_t, size_t>> mm{};
        for (size_t i = 0; i < target_node->mNumMeshes; i++)
        {
            auto mesh_index = target_node->mMeshes[i];
            meshInstances[mesh_index]++;
            auto material_index = scene->mMeshes[mesh_index]->mMaterialIndex;
            mm.emplace_back(mesh_index, material_index);
        }

        for (unsigned int li = 0; li < scene->mNumLights; ++li)
        {
            if (scene->mLights[li]->mName != target_node->mName) continue;

            ProcessLight(dirLights, pointLights, scene->mLights[li], transform);
        }

        out.emplace_back(transform, mm);

        for (size_t i = 0; i < target_node->mNumChildren; i++)
        {
            ProcessNodesRecursive(out, dirLights, meshInstances, pointLights, scene, target_node->mChildren[i], transform);
        }
    }

    Material Model::ProcessMaterial(const std::vector<std::string>& image_paths, const aiMaterial* material)
    {
        using namespace MaterialConstants;
        Material out;

        // Base colour factor
        if (aiColor4D t{}; material->Get(AI_MATKEY_BASE_COLOR, t) == aiReturn_SUCCESS)
        {
            out.AddParameter(BASE_COLOUR_FACTOR_NAME, glm::vec4{t.r, t.g, t.b, t.a});
        }
        else
        {
            out.AddParameter(BASE_COLOUR_FACTOR_NAME, glm::vec4{t.r, t.g, t.b, t.a});
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
            aiString texture_name{};
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
                LOG(Log::Severity::WARN, "Support for non embedded textures is still very limited");
                return {};
            }
        };

        auto baseColorTex = GetTexture(aiTextureType_BASE_COLOR);
        if (baseColorTex)
        {
            out.AddParameter(BASE_TEXTURE_NAME, ResourceHandle<Texture>{baseColorTex.value()});
        }
        else
        {
            out.AddParameter(BASE_TEXTURE_NAME, ResourceHandle<Texture>{"assets/textures/White.png"});
        }

        if (auto path = GetTexture(aiTextureType_NORMALS))
        {
            out.AddParameter(NORMAL_TEXTURE_NAME, ResourceHandle<Texture>{path.value()});
        }
        else
        {
            out.AddParameter(NORMAL_TEXTURE_NAME, ResourceHandle<Texture>{"assets/textures/Blue.jpg"});
        }

        if (auto path = GetTexture(aiTextureType_LIGHTMAP))
        {
            out.AddParameter(OCCLUSION_TEXTURE_NAME, ResourceHandle<Texture>{path.value()});
        }
        else
        {
            out.AddParameter(OCCLUSION_TEXTURE_NAME, ResourceHandle<Texture>{"assets/textures/White.png"});
        }

        if (auto path = GetTexture(aiTextureType_METALNESS))
        {
            out.AddParameter(METALLIC_TEXTURE_NAME, ResourceHandle<Texture>{path.value()});
        }
        else
        {
            out.AddParameter(METALLIC_TEXTURE_NAME, ResourceHandle<Texture>{"assets/textures/Black.png"});
        }

        if (auto path = GetTexture(aiTextureType_EMISSIVE))
        {
            out.AddParameter(EMISSIVE_TEXTURE_NAME, ResourceHandle<Texture>{path.value()});
        }
        else
        {
            out.AddParameter(EMISSIVE_TEXTURE_NAME, ResourceHandle<Texture>{"assets/textures/Black.png"});
        }

        return out;
    }
}

std::optional<KS::ResourceHandle<KS::Model>> KS::ModelImporter::ImportFromFile(const FileIO::Path& source_model,
                                                                               uint32_t post_processing_flags)
{
    Assimp::Importer importer;
    const aiScene* scene = nullptr;
    LOG(Log::Severity::INFO, "Importing model file: {}", source_model.string());

    auto source = source_model;
    auto base_dir = source.make_preferred().parent_path();
    auto out_dir = base_dir / source.stem();
    auto out_model_file = out_dir / (source.filename().replace_extension().string() + ".assbin");

    if (!std::filesystem::exists(out_model_file))
    {
        FileIO::MakeDirectory(out_dir);
        auto file_data = FileIO::OpenReadStream(source_model, std::ios::binary);
        if (!file_data)
        {
            LOG(Log::Severity::WARN, "Could not open file: {}", source_model.string());
            return {};
        }

        auto dump = FileIO::DumpFullStream(file_data.value());
        scene = importer.ReadFileFromMemory(dump.data(), dump.size(), post_processing_flags);

        if (!scene)
        {
            LOG(Log::Severity::WARN, "Could not import model: {} ({})", source_model.string(), importer.GetErrorString());
            return {};
        }

        // Export a cached assbin for next time
        Assimp::Exporter exporter;
        aiReturn res = exporter.Export(scene, "assbin", out_model_file.string());
        if (res != aiReturn_SUCCESS)
        {
            LOG(Log::Severity::WARN, "Export to assbin failed for {}: {}", out_model_file.string(), exporter.GetErrorString());
        }

    }

    return ResourceHandle<Model>{source_model.string()};

}
