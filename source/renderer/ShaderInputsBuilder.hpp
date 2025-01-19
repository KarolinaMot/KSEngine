#pragma once
#include "Sampler.hpp"
#include "ShaderInputs.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace KS
{
class Device;
class ShaderInputsBuilder
{
public:
    ShaderInputsBuilder();
    ~ShaderInputsBuilder();
    ShaderInputsBuilder& AddUniform(ShaderInputVisibility visibility, std::string name);
    ShaderInputsBuilder& AddStorageBuffer(ShaderInputVisibility visibility, int numberOfElements, std::string name, ShaderInputMod modifiable = ShaderInputMod::READ_ONLY);
    ShaderInputsBuilder& AddTexture(ShaderInputVisibility visibility, std::string name, ShaderInputMod modifiable = ShaderInputMod::READ_ONLY);
    ShaderInputsBuilder& AddStaticSampler(ShaderInputVisibility visibility, SamplerDesc samplerDesc);
    ShaderInputsBuilder& AddHeapRanges(std::vector<std::tuple<unsigned int, /* BaseShaderRegister, */ unsigned int, /* NumDescriptors */ unsigned int,
                                           /* RegisterSpace */ InputType,
                                           /* RangeType */ unsigned int /* OffsetInDescriptorsFromTableStart */>>
                                           ranges,
        std::string descriptorName);

    std::shared_ptr<ShaderInputs> Build(const Device& device, std::string name);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    std::unordered_map<std::string, ShaderInput> m_descriptors;
    std::vector<std::pair<ShaderInputVisibility, SamplerDesc>> m_sampler_inputs;
    int m_buffer_counter = 0;
    int m_ro_array_counter = 0;
    int m_rw_array_counter = 0;
    int m_texture_counter = 0;
    int m_input_counter = 0;
};

}