#include "ShaderInputsBuilder.hpp"

KS::ShaderInputsBuilder &KS::ShaderInputsBuilder::AddStruct(ShaderInputVisibility visibility, std::string name)
{
    KS::ShaderInput input;
    input.rootIndex = m_input_counter;
    input.type = InputType::BUFFER;
    input.typeIndex = m_buffer_counter;
    input.visibility = visibility;
    m_input_counter++;
    m_buffer_counter++;

    m_descriptors[name] = input;
    return *this;
}

KS::ShaderInputsBuilder &KS::ShaderInputsBuilder::AddModifiableStructs(ShaderInputVisibility visibility, int numberOfElements, std::string name, ShaderInputMod modifiable)
{
    KS::ShaderInput input;
    input.modifications = modifiable;
    input.numberOfElements = numberOfElements;
    input.rootIndex = m_input_counter;
    input.type = modifiable == ShaderInputMod::READ_ONLY ? InputType::RO_DATA : InputType::RW_DATA;
    input.typeIndex = modifiable == ShaderInputMod::READ_ONLY ? m_ro_array_counter : m_rw_array_counter;
    input.visibility = visibility;
    m_descriptors[name] = input;

    m_input_counter++;
    if (modifiable == ShaderInputMod::READ_ONLY)
        m_ro_array_counter++;
    else
        m_rw_array_counter++;

    return *this;
}

KS::ShaderInputsBuilder &KS::ShaderInputsBuilder::AddTexture(ShaderInputVisibility visibility, std::string name, ShaderInputMod modifiable)
{
    KS::ShaderInput input;
    input.modifications = modifiable;
    input.rootIndex = m_input_counter;
    input.type = modifiable == ShaderInputMod::READ_ONLY ? InputType::RO_DATA : InputType::RW_DATA;
    input.typeIndex = modifiable == ShaderInputMod::READ_ONLY ? m_ro_array_counter : m_rw_array_counter;
    input.visibility = visibility;
    m_descriptors[name] = input;

    m_input_counter++;
    if (modifiable == ShaderInputMod::READ_ONLY)
        m_ro_array_counter++;
    else
        m_rw_array_counter++;

    return *this;
}

KS::ShaderInputsBuilder &KS::ShaderInputsBuilder::AddStaticSampler(ShaderInputVisibility visibility, SamplerDesc samplerDesc)
{
    std::pair<ShaderInputVisibility, SamplerDesc> sampler = {visibility, samplerDesc};
    m_sampler_inputs.push_back(sampler);

    return *this;
}

std::shared_ptr<KS::ShaderInputs> KS::ShaderInputsBuilder::Build(const Device &device, std::string name)
{
    return std::make_shared<ShaderInputs>(device, std::move(m_descriptors), m_sampler_inputs, m_ro_array_counter + m_rw_array_counter, name);
}
