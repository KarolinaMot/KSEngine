#pragma once
#include "Sampler.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace KS
{
enum class ShaderInputVisibility
{
    VERTEX,
    PIXEL,
    COMPUTE
};

enum class ShaderInputMod
{
    READ_ONLY,
    READ_WRITE
};

enum class InputType
{
    BUFFER,
    RW_DATA,
    RO_DATA,
    RANGE
};

struct ShaderInput
{
    ShaderInputVisibility visibility;
    ShaderInputMod modifications;
    InputType type;
    int numberOfElements;
    int rootIndex;
    int typeIndex;
};

class Device;
class ShaderInputs
{
public:
    ShaderInputs(const Device& device, std::unordered_map<std::string, ShaderInput>&& inputs, const std::vector<std::pair<ShaderInputVisibility, SamplerDesc>>& samplers,  int totalDataTypeCount, std::string name);
    ShaderInputs(const Device& device, std::unordered_map<std::string, ShaderInput>&& inputs, void* signature, std::string name);
    ~ShaderInputs();
    void* GetSignature() const;
    ShaderInput GetInput(std::string key) const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    std::unordered_map<std::string, ShaderInput> m_descriptors;
};
}