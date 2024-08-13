#include <renderer/ShaderInputsBuilder.hpp>
#include <renderer/DX12/Helpers/DXSignature.hpp>
#include <Device/Device.hpp>

class KS::ShaderInputsBuilder::Impl
{
public:
    DXSignatureBuilder m_builder;
    D3D12_SHADER_VISIBILITY GetVisibility(ShaderInputVisibility visibility);
};

KS::ShaderInputsBuilder::ShaderInputsBuilder()
{
    m_impl = std::make_unique<Impl>();
    m_impl->m_builder = DXSignatureBuilder(20);
}

KS::ShaderInputsBuilder::~ShaderInputsBuilder()
{
}

KS::ShaderInputsBuilder& KS::ShaderInputsBuilder::AddUniform(ShaderInputVisibility visibility, std::string name)
{
    KS::ShaderInput input;
    input.rootIndex = m_input_counter;
    input.type = InputType::BUFFER;
    input.typeIndex = m_buffer_counter;
    input.visibility = visibility;

    m_impl->m_builder.AddCBuffer(m_buffer_counter, m_impl->GetVisibility(visibility));
    m_input_counter++;
    m_buffer_counter++;

    m_descriptors[name] = input;
    return *this;
}

KS::ShaderInputsBuilder& KS::ShaderInputsBuilder::AddStorageBuffer(ShaderInputVisibility visibility, int numberOfElements, std::string name, ShaderInputMod modifiable)
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
    {
        m_impl->m_builder.AddTable(m_impl->GetVisibility(visibility), D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, m_ro_array_counter);
        m_ro_array_counter++;
    }
    else
    {
        m_impl->m_builder.AddTable(m_impl->GetVisibility(visibility), D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, m_rw_array_counter);
        m_rw_array_counter++;
    }

    return *this;
}

KS::ShaderInputsBuilder& KS::ShaderInputsBuilder::AddTexture(ShaderInputVisibility visibility, std::string name, ShaderInputMod modifiable)
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
    {
        m_impl->m_builder.AddTable(m_impl->GetVisibility(visibility), D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, m_ro_array_counter);
        m_ro_array_counter++;
    }
    else
    {
        m_impl->m_builder.AddTable(m_impl->GetVisibility(visibility), D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, m_rw_array_counter);
        m_rw_array_counter++;
    }

    return *this;
}

KS::ShaderInputsBuilder& KS::ShaderInputsBuilder::AddStaticSampler(ShaderInputVisibility visibility, SamplerDesc samplerDesc)
{
    std::pair<ShaderInputVisibility, SamplerDesc> sampler = { visibility, samplerDesc };
    D3D12_TEXTURE_ADDRESS_MODE addressMode;

    switch (sampler.second.addressMode)
    {
    case SamplerAddressMode::SAM_CLAMP:
        addressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        break;
    case SamplerAddressMode::SAM_MIRROR:
        addressMode = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        break;
    case SamplerAddressMode::SAM_BORDER:
        addressMode = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        break;
    case SamplerAddressMode::SAM_MIRROR_ONCE:
        addressMode = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
        break;
    case SamplerAddressMode::SAM_WRAP:
        addressMode = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        break;
    }

    D3D12_FILTER filterMode;
    switch (sampler.second.filter)
    {
    case SamplerFilter::SF_NEAREST:
        filterMode = D3D12_FILTER_MIN_MAG_MIP_POINT;
        break;

    case SamplerFilter::SF_LINEAR:
        filterMode = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        break;

    case SamplerFilter::SF_ANISOTROPIC:
        filterMode = D3D12_FILTER_ANISOTROPIC;
        break;
    }

    D3D12_STATIC_BORDER_COLOR borderColor;
    switch (sampler.second.borderColor)
    {
    case SamplerBorderColor::SBC_TRANSPARENT_BLACK:
        borderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        break;
    case SamplerBorderColor::SBC_OPAQUE_BLACK:
        borderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
        break;
    case SamplerBorderColor::SBC_OPAQUE_WHITE:
        borderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
        break;
    }

    m_impl->m_builder.AddSampler(m_sampler_inputs.size(), m_impl->GetVisibility(sampler.first), addressMode, filterMode, borderColor);
    m_sampler_inputs.push_back(sampler);

    return *this;
}

std::shared_ptr<KS::ShaderInputs> KS::ShaderInputsBuilder::Build(const Device& device, std::string name)
{
    wchar_t wString[4096];
    MultiByteToWideChar(CP_ACP, 0, name.c_str(), -1, wString, 4096);

    return std::make_shared<ShaderInputs>(device, std::move(m_descriptors),
        m_impl->m_builder.Build(reinterpret_cast<ID3D12Device5*>(device.GetDevice()), wString).Get(),
        name);
}

D3D12_SHADER_VISIBILITY KS::ShaderInputsBuilder::Impl::GetVisibility(ShaderInputVisibility visibility)
{
    D3D12_SHADER_VISIBILITY descVisibility;
    switch (visibility)
    {
    case ShaderInputVisibility::PIXEL:
        descVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        break;
    case ShaderInputVisibility::VERTEX:
        descVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        break;
    case ShaderInputVisibility::COMPUTE:
        descVisibility = D3D12_SHADER_VISIBILITY_ALL;
        break;
    }

    return descVisibility;
}
