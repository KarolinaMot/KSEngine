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
    ShaderInputVisibility visibility;
    ShaderInputMod modifications;
    InputType type;
    int numberOfElements;
    int rootIndex;
    int typeIndex;
};

class Device;
class ShaderInputCollection
{
public:
    ShaderInputCollection(std::unordered_map<std::string, ShaderInputDesc>&& inputs, void* signature, std::string name, bool global = true);
    ~ShaderInputCollection();
    void* GetSignature() const;
    ShaderInputDesc GetInput(std::string key) const;
    bool GetIsGlobal() const { return m_global; }

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    std::unordered_map<std::string, ShaderInputDesc> m_descriptors;
    bool m_global = true;
};
}