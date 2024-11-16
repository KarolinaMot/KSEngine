#pragma once

#include <Common.hpp>
#include <SerializationCommon.hpp>

#include <glm/vec4.hpp>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

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