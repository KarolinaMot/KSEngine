#include <resources/Material.hpp>

#include <FileIO.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/variant.hpp>
#include <serialization/MathTypes.hpp>

void Material::AddParameter(const std::string& key, const InputParameter& param)
{
    input_parameters.emplace(key, param);
}

const Material::InputParameter* Material::GetParameter(const std::string& key) const
{
    if (auto it = input_parameters.find(key); it != input_parameters.end())
    {
        return &it->second;
    }

    return nullptr;
}

std::optional<uint32_t> Material::GetParameterI(const std::string& key) const
{
    auto p = GetParameter(key);
    if (p == nullptr)
        return std::nullopt;

    auto v = std::get_if<uint32_t>(p);
    if (v == nullptr)
        return std::nullopt;

    return *v;
}

std::optional<glm::vec4> Material::GetParameterV(const std::string& key) const
{
    auto p = GetParameter(key);
    if (p == nullptr)
        return std::nullopt;

    auto v = std::get_if<glm::vec4>(p);
    if (v == nullptr)
        return std::nullopt;

    return *v;
}

std::optional<std::string_view> Material::GetParameterT(const std::string& key) const
{
    auto p = GetParameter(key);
    if (p == nullptr)
        return std::nullopt;

    auto v = std::get_if<std::string>(p);
    if (v == nullptr)
        return std::nullopt;

    return *v;
}

void Material::save(JSONSaver& ar, const uint32_t v) const
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

void Material::load(JSONLoader& ar, const uint32_t v)
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

std::optional<Material> MaterialUtility::LoadMaterialFromFile(const std::string& path)
{
    auto stream = FileIO::OpenReadStream(path);

    if (!stream)
        return std::nullopt;

    JSONLoader loader { stream.value() };

    Material out {};
    loader(out);
    return out;
}