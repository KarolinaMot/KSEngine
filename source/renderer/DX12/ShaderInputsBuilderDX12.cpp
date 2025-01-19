#include <renderer/ShaderInputsBuilder.hpp>
#include <renderer/DX12/Helpers/DXSignature.hpp>
#include <device/Device.hpp>

class KS::ShaderInputsBuilder::Impl
{
public:
    int mRangeCounter = 0;
    std::vector<D3D12_DESCRIPTOR_RANGE> mRanges;
    std::vector<D3D12_ROOT_PARAMETER> mParameters;
    std::vector<D3D12_STATIC_SAMPLER_DESC> mSamplers;

    D3D12_SHADER_VISIBILITY GetVisibility(ShaderInputVisibility visibility);
    void AddCBuffer(const uint32_t shaderRegister, D3D12_SHADER_VISIBILITY shader);
    void AddTable(D3D12_SHADER_VISIBILITY shader, D3D12_DESCRIPTOR_RANGE_TYPE rangeType, int numDescriptors, int shaderRegister);
    void AddSampler(const uint32_t shaderRegister, D3D12_SHADER_VISIBILITY shader, D3D12_TEXTURE_ADDRESS_MODE mode, D3D12_FILTER filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT, D3D12_STATIC_BORDER_COLOR color = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK, D3D12_COMPARISON_FUNC comparison = D3D12_COMPARISON_FUNC_NEVER);
};

KS::ShaderInputsBuilder::ShaderInputsBuilder()
{
    m_impl = std::make_unique<Impl>();
    m_impl->mRanges.resize(20);
}

KS::ShaderInputsBuilder::~ShaderInputsBuilder()
{
}

void KS::ShaderInputsBuilder::Impl::AddCBuffer(const uint32_t shaderRegister, D3D12_SHADER_VISIBILITY shader)
{
    D3D12_ROOT_DESCRIPTOR desc;
    desc.RegisterSpace = 0;
    desc.ShaderRegister = shaderRegister;

    D3D12_ROOT_PARAMETER par;
    par.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    par.Descriptor = desc;
    par.ShaderVisibility = shader;

    mParameters.push_back(par);
}

void KS::ShaderInputsBuilder::Impl::AddTable(D3D12_SHADER_VISIBILITY shader, D3D12_DESCRIPTOR_RANGE_TYPE rangeType, int numDescriptors, int shaderRegister)
{
    D3D12_DESCRIPTOR_RANGE range;
    range.RangeType = rangeType;
    range.NumDescriptors = numDescriptors;
    range.BaseShaderRegister = shaderRegister;
    range.RegisterSpace = 0;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    mRanges[mRangeCounter] = range;

    D3D12_ROOT_DESCRIPTOR_TABLE descriptorTable;
    descriptorTable.NumDescriptorRanges = 1;
    descriptorTable.pDescriptorRanges = &mRanges[mRangeCounter];
    mRangeCounter++;

    D3D12_ROOT_PARAMETER par {};
    par.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    par.DescriptorTable = descriptorTable;
    par.ShaderVisibility = shader;
    mParameters.push_back(par);
}

void KS::ShaderInputsBuilder::Impl::AddSampler(const uint32_t shaderRegister, D3D12_SHADER_VISIBILITY shader, D3D12_TEXTURE_ADDRESS_MODE mode, D3D12_FILTER filter, D3D12_STATIC_BORDER_COLOR color, D3D12_COMPARISON_FUNC comparison)
{
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = filter;
    sampler.AddressU = mode;
    sampler.AddressV = mode;
    sampler.AddressW = mode;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 0;
    sampler.ComparisonFunc = comparison;
    sampler.BorderColor = color;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = shaderRegister;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = shader;

    mSamplers.push_back(sampler);
}

KS::ShaderInputsBuilder& KS::ShaderInputsBuilder::AddUniform(ShaderInputVisibility visibility, std::string name)
{
    KS::ShaderInput input;
    input.rootIndex = m_input_counter;
    input.type = InputType::BUFFER;
    input.typeIndex = m_buffer_counter;
    input.visibility = visibility;

    m_impl->AddCBuffer(m_buffer_counter, m_impl->GetVisibility(visibility));

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
        m_impl->AddTable(m_impl->GetVisibility(visibility), D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, m_ro_array_counter);
        m_ro_array_counter++;
    }
    else
    {
        m_impl->AddTable(m_impl->GetVisibility(visibility), D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, m_rw_array_counter);
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
        m_impl->AddTable(m_impl->GetVisibility(visibility), D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, m_ro_array_counter);
        m_ro_array_counter++;
    }
    else
    {
        m_impl->AddTable(m_impl->GetVisibility(visibility), D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, m_rw_array_counter);
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

    m_impl->AddSampler(m_sampler_inputs.size(), m_impl->GetVisibility(sampler.first), addressMode, filterMode, borderColor);
    m_sampler_inputs.push_back(sampler);

    return *this;
}

std::shared_ptr<KS::ShaderInputs> KS::ShaderInputsBuilder::Build(const Device& device, std::string name)
{
    wchar_t wString[4096];
    MultiByteToWideChar(CP_ACP, 0, name.c_str(), -1, wString, 4096);

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(static_cast<UINT>(m_impl->mParameters.size()),
        m_impl->mParameters.data(),
        static_cast<UINT>(m_impl->mSamplers.size()),
        m_impl->mSamplers.data(),
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);

    ComPtr<ID3DBlob> serializedSignature;
    ComPtr<ID3DBlob> errBlob;

    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedSignature, &errBlob);

    if (FAILED(hr))
    {
        LOG(Log::Severity::WARN, "{}",
            (const char*)errBlob->GetBufferPointer());
        ASSERT(false && "Failed to serialize root signature");
    }
    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());

    ComPtr<ID3D12RootSignature> signature;
    hr = engineDevice->CreateRootSignature(0, serializedSignature->GetBufferPointer(), serializedSignature->GetBufferSize(), IID_PPV_ARGS(&signature));

    if (FAILED(hr))
    {
        MessageBox(NULL, "Failed to initialize root signature", "FATAL ERROR!", MB_ICONERROR | MB_OK);
        ASSERT(false && "Failed to initialize root signature");
    }
    signature->SetName(wString);

    return std::make_shared<ShaderInputs>(device, std::move(m_descriptors),
        signature.Get(),
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

KS::ShaderInputsBuilder& KS::ShaderInputsBuilder::AddHeapRanges(std::vector<std::tuple<unsigned int, /* BaseShaderRegister, */ unsigned int, /* NumDescriptors */ unsigned int,
                                                                    /* RegisterSpace */ InputType,
                                                                    /* RangeType */ unsigned int /* OffsetInDescriptorsFromTableStart */>>
                                                                    ranges,
    std::string descriptorName)
{
    // for (const auto& input : ranges)
    // {
    //     D3D12_DESCRIPTOR_RANGE range;
    //     range.RangeType = std::get<3>(input) == InputType::BUFFER || std::get<3>(input) == InputType::RO_DATA ? D3D12_DESCRIPTOR_RANGE_TYPE_SRV : D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    //     range.NumDescriptors = std::get<1>(input);
    //     range.BaseShaderRegister = std::get<0>(input);
    //     range.RegisterSpace = 0;
    //     range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    //     if (range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
    //         m_ro_array_counter++;
    //     else
    //         m_rw_array_counter++;

    //     m_impl->mRanges[m_impl->mRangeCounter] = range;
    //     m_impl->mRangeCounter++;
    // }

    // KS::ShaderInput input;
    // input.rootIndex = m_input_counter;
    // input.type = InputType::RANGE;
    // m_descriptors[descriptorName] = input;
    // m_input_counter += ranges.size();

    // D3D12_ROOT_PARAMETER param = {};
    // param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    // param.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(ranges.size());
    // param.DescriptorTable.pDescriptorRanges = nullptr;
    // m_impl->mParameters.push_back(param);
}
