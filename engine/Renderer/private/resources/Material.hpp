#pragma once

#include <Common.hpp>

#include <cereal/types/variant.hpp>
#include <optional>
#include <resources/Serialization.hpp>
#include <variant>

namespace MaterialConstants
{

// BASE COLOUR
const std::string BASE_COLOUR_FACTOR_NAME = "BASE_COLOUR";
constexpr glm::vec4 BASE_COLOUR_FACTOR_DEFAULT = { 1.0f, 1.0f, 1.0f, 1.0f };

// NORMAL, EMISSIVE, ALPHA CUTOFF
const std::string NEA_FACTORS_NAME = "NEA_FACTOR";
constexpr glm::vec4 NEA_FACTORS_DEFAULT = { 1.0f, 1.0f, 0.0f, 0.0f };

// OCCLUSION, ROUGHNESS, METALLIC
const std::string ORM_FACTORS_NAME = "ORM_FACTORS";
constexpr glm::vec4 ORM_FACTORS_DEFAULT = { 1.0f, 0.5f, 0.5f, 0.0f };

const std::string DOUBLE_SIDED_FLAG_NAME = "DOUBLE_SIDED";
constexpr bool DOUBLE_SIDED_DEFAULT = false;

const std::string BASE_TEXTURE_NAME = "BASE_TEXTURE";
const std::string NORMAL_TEXTURE_NAME = "NORMAL_TEXTURE";
const std::string OCCLUSION_TEXTURE_NAME = "OCCLUSION_TEXTURE";
const std::string METALLIC_TEXTURE_NAME = "METALLIC_TEXTURE";
const std::string EMISSIVE_TEXTURE_NAME = "EMISSIVE_TEXTURE";

}

class Texture;

class Material
{
public:
    using InputParameter = std::variant<bool, int, float, glm::vec4, ResourceHandle<Texture>>;

    Material() = default;

    // Input parameters can be flags (bool), integers, scalars (vec4 or floats) or paths to textures
    void AddParameter(const std::string& key, const InputParameter& param);

    // Warning: can be null, check pointer before using
    template <typename T>
    const T* GetParameter(const std::string& key) const
    {
        if (auto it = input_parameters.find(key); it != input_parameters.end())
        {
            return std::get_if<T>(&it->second);
        }

        return nullptr;
    }

private:
    friend class cereal::access;

    template <typename A>
    void save(A& ar, const uint32_t v) const;

    template <typename A>
    void load(A& ar, const uint32_t v);

    std::map<std::string, InputParameter> input_parameters;
};

template <typename A>
inline void Material::save(A& ar, const uint32_t v) const
{
    switch (v)
    {
    case 0:
        ar(cereal::make_nvp("Parameters", input_parameters));
        break;

    default:
        break;
    }
}

template <typename A>
inline void Material::load(A& ar, const uint32_t v)
{
    switch (v)
    {
    case 0:
        ar(cereal::make_nvp("Parameters", input_parameters));
        break;

    default:
        break;
    }
}

CEREAL_CLASS_VERSION(Material, 0);