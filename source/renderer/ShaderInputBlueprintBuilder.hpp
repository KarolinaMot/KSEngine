#pragma once
#include "Sampler.hpp"
#include "ShaderInputBlueprint.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace KS
{
class Device;
class ShaderInputBlueprintBuilder
{
public:
    ShaderInputBlueprintBuilder();
    ~ShaderInputBlueprintBuilder();
    ShaderInputBlueprintBuilder& AddUniform(ShaderInputVisibility visibility, const std::initializer_list<std::string>& names,
                                            uint32_t space = 0);
    ShaderInputBlueprintBuilder& AddStorageBuffer(ShaderInputVisibility visibility, int numberOfElements, std::string name,
                                                  ShaderInputMod modifiable = ShaderInputMod::READ_ONLY, uint32_t space = 0);
    ShaderInputBlueprintBuilder& AddTexture(ShaderInputVisibility visibility, std::string name,
                                            ShaderInputMod modifiable = ShaderInputMod::READ_ONLY, uint32_t space = 0);
    ShaderInputBlueprintBuilder& AddStaticSampler(ShaderInputVisibility visibility, SamplerDesc samplerDesc,
                                                  uint32_t space = 0);
    ShaderInputBlueprintBuilder& SetLocal()
    {
        m_local = true;
        return *this;
    };

    std::shared_ptr<ShaderInputBlueprint> Build(const Device& device, std::string name);

    struct SpaceState
    {
        uint32_t next[(int)InputType::Count] = {0, 0, 0, 0};                // next t/u/b/s
        bool sealed[(int)InputType::Count] = {false, false, false, false};  // set when unbounded is used
    };

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    std::unordered_map<std::string, ShaderInputDesc> m_descriptors;
    std::vector<std::pair<ShaderInputVisibility, SamplerDesc>> m_sampler_inputs;
    std::unordered_map<uint32_t, SpaceState> spaces;

    //int m_buffer_counter = 0;
    //int m_ro_array_counter = 0;
    //int m_rw_array_counter = 0;

    int m_input_counter = 0;
    bool m_local = false;
};

}  // namespace KS