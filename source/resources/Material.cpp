#include "Material.hpp"

void KS::Material::AddParameter(const std::string& key, const InputParameter& param)
{
    input_parameters.emplace(key, param);
}