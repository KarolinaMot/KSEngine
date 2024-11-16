#pragma once

#include <Common.hpp>
#include <SerializationCommon.hpp>

#include <glm/vec4.hpp>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace MaterialConstants
{

// BASE COLOUR
constexpr auto BASE_COLOUR_FACTOR_NAME = "BASE_COLOUR";
constexpr glm::vec4 BASE_COLOUR_FACTOR_DEFAULT = { 1.0f, 1.0f, 1.0f, 1.0f };

// NORMAL, EMISSIVE, ALPHA CUTOFF
constexpr auto NEA_FACTORS_NAME = "NEA_FACTOR";
constexpr glm::vec4 NEA_FACTORS_DEFAULT = { 1.0f, 1.0f, 0.0f, 0.0f };

// OCCLUSION, ROUGHNESS, METALLIC
constexpr auto ORM_FACTORS_NAME = "ORM_FACTORS";
constexpr glm::vec4 ORM_FACTORS_DEFAULT = { 1.0f, 0.5f, 0.5f, 0.0f };

constexpr auto DOUBLE_SIDED_FLAG_NAME = "DOUBLE_SIDED";
constexpr bool DOUBLE_SIDED_DEFAULT = false;

constexpr auto BASE_TEXTURE_NAME = "BASE_TEXTURE";
constexpr auto NORMAL_TEXTURE_NAME = "NORMAL_TEXTURE";
constexpr auto OCCLUSION_TEXTURE_NAME = "OCCLUSION_TEXTURE";
constexpr auto METALLIC_TEXTURE_NAME = "METALLIC_TEXTURE";
constexpr auto EMISSIVE_TEXTURE_NAME = "EMISSIVE_TEXTURE";
}

class Material
{
public:
    using InputParameter = std::variant<uint32_t, glm::vec4, std::string>;

    Material() = default;
    void AddParameter(const std::string& key, const InputParameter& param);

    // Returns a unsigned integer if present
    std::optional<uint32_t> GetParameterI(const std::string& key) const;

    // Returns a vec4 if present
    std::optional<glm::vec4> GetParameterV(const std::string& key) const;

    // Returns a texture path if present
    std::optional<std::string_view> GetParameterT(const std::string& key) const;

private:
    const InputParameter* GetParameter(const std::string& key) const;
    std::map<std::string, InputParameter> input_parameters;

    friend class cereal::access;
    void save(JSONSaver& ar, const uint32_t v) const;
    void load(JSONLoader& ar, const uint32_t v);
};

CEREAL_CLASS_VERSION(Material, 0);

namespace MaterialUtility
{
std::optional<Material> LoadMaterialFromFile(const std::string& path);
}