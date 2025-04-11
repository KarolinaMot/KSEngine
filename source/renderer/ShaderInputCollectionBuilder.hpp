#pragma once
#include "Sampler.hpp"
#include "ShaderInputCollection.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace KS
{
class Device;
class ShaderInputCollectionBuilder
{
public:
    ShaderInputCollectionBuilder();
    ~ShaderInputCollectionBuilder();
    ShaderInputCollectionBuilder& AddUniform(ShaderInputVisibility visibility, const std::initializer_list<std::string>& names);
    ShaderInputCollectionBuilder& AddStorageBuffer(ShaderInputVisibility visibility, int numberOfElements, std::string name,
                                          ShaderInputMod modifiable = ShaderInputMod::READ_ONLY);
    ShaderInputCollectionBuilder& AddTexture(ShaderInputVisibility visibility, std::string name,
                                    ShaderInputMod modifiable = ShaderInputMod::READ_ONLY);
    ShaderInputCollectionBuilder& AddStaticSampler(ShaderInputVisibility visibility, SamplerDesc samplerDesc);
    std::shared_ptr<ShaderInputCollection> Build(const Device& device, std::string name);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    std::unordered_map<std::string, ShaderInputDesc> m_descriptors;
    std::vector<std::pair<ShaderInputVisibility, SamplerDesc>> m_sampler_inputs;
    int m_buffer_counter = 0;
    int m_ro_array_counter = 0;
    int m_rw_array_counter = 0;
    int m_texture_counter = 0;
    int m_input_counter = 0;
};

}  // namespace KS