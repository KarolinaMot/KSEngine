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

struct ShaderInputDesc
{
    ShaderInputVisibility visibility{};
    ShaderInputMod modifications{};
    InputType type{};
    int numberOfElements = 0;
    int rootIndex = 0;
    int typeIndex = 0;
};

struct ShaderInputBindDesc
{
    ShaderInputBindDesc() = default;
    ShaderInputBindDesc(uint32_t bindOff, ShaderInputDesc description) : bindOffset(bindOff), desc(description){};
    ShaderInputBindDesc(ShaderInputDesc description) : desc(description){};
    uint32_t bindOffset = 0;
    ShaderInputDesc desc{};
};

class Device;
class ShaderInputBlueprint
{
public:
    ShaderInputBlueprint(const Device& device, std::unordered_map<std::string, ShaderInputDesc>&& inputs, void* signature, bool global, std::string name);
    ~ShaderInputBlueprint();
    void* GetSignature() const;
    ShaderInputDesc GetInput(std::string key) const;
    bool GetIsGlobal() const { return m_isGlobal; }

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    std::unordered_map<std::string, ShaderInputDesc> m_descriptors;
    bool m_isGlobal = true;
};
}