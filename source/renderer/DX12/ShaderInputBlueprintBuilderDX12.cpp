#include <device/Device.hpp>
#include <renderer/DX12/Helpers/DXIncludes.hpp>
#include <renderer/ShaderInputBlueprintBuilder.hpp>

class KS::ShaderInputBlueprintBuilder::Impl
{
public:
    int mRangeCounter = 0;
    std::vector<D3D12_DESCRIPTOR_RANGE> mRanges;
    std::vector<D3D12_ROOT_PARAMETER> mParameters;
    std::vector<D3D12_STATIC_SAMPLER_DESC> mSamplers;

    D3D12_SHADER_VISIBILITY GetVisibility(ShaderInputVisibility visibility);
    void AddCBuffer(const uint32_t shaderRegister, D3D12_SHADER_VISIBILITY shader, int registerSpace=0);
    void AddTable(D3D12_SHADER_VISIBILITY shader, D3D12_DESCRIPTOR_RANGE_TYPE rangeType, int numDescriptors, int shaderRegister,
                  int registerSpace=0);
    void AddSampler(const uint32_t shaderRegister, D3D12_SHADER_VISIBILITY shader, D3D12_TEXTURE_ADDRESS_MODE mode,
                    D3D12_FILTER filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,
                    D3D12_STATIC_BORDER_COLOR color = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
                    D3D12_COMPARISON_FUNC comparison = D3D12_COMPARISON_FUNC_NEVER, int registerSpace=0);
};

KS::ShaderInputBlueprintBuilder::ShaderInputBlueprintBuilder()
{
    m_impl = std::make_unique<Impl>();
    m_impl->mRanges.resize(20);
}

KS::ShaderInputBlueprintBuilder::~ShaderInputBlueprintBuilder() {}

void KS::ShaderInputBlueprintBuilder::Impl::AddCBuffer(const uint32_t shaderRegister, D3D12_SHADER_VISIBILITY shader,
                                                       int registerSpace)
{
    D3D12_ROOT_DESCRIPTOR desc;
    desc.RegisterSpace = registerSpace;
    desc.ShaderRegister = shaderRegister;

    D3D12_ROOT_PARAMETER par;
    par.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    par.Descriptor = desc;
    par.ShaderVisibility = shader;

    mParameters.push_back(par);
}

void KS::ShaderInputBlueprintBuilder::Impl::AddTable(D3D12_SHADER_VISIBILITY shader, D3D12_DESCRIPTOR_RANGE_TYPE rangeType,
                                             int numDescriptors, int shaderRegister, int registerSpace)
{
    D3D12_DESCRIPTOR_RANGE range;
    range.RangeType = rangeType;
    range.NumDescriptors = numDescriptors;
    range.BaseShaderRegister = shaderRegister;
    range.RegisterSpace = registerSpace;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    mRanges[mRangeCounter] = range;

    D3D12_ROOT_DESCRIPTOR_TABLE descriptorTable;
    descriptorTable.NumDescriptorRanges = 1;
    descriptorTable.pDescriptorRanges = &mRanges[mRangeCounter];
    mRangeCounter++;

    D3D12_ROOT_PARAMETER par{};
    par.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    par.DescriptorTable = descriptorTable;
    par.ShaderVisibility = shader;
    mParameters.push_back(par);
}

void KS::ShaderInputBlueprintBuilder::Impl::AddSampler(const uint32_t shaderRegister, D3D12_SHADER_VISIBILITY shader,
                                               D3D12_TEXTURE_ADDRESS_MODE mode, D3D12_FILTER filter,
                                                       D3D12_STATIC_BORDER_COLOR color, D3D12_COMPARISON_FUNC comparison,
                                                       int registerSpace)
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
    sampler.RegisterSpace = registerSpace;
    sampler.ShaderVisibility = shader;

    mSamplers.push_back(sampler);
}

KS::ShaderInputBlueprintBuilder& KS::ShaderInputBlueprintBuilder::AddUniform(ShaderInputVisibility visibility,
                                                                             const std::initializer_list<std::string>& names,
                                                                             uint32_t space)
{
    KS::ShaderInputDesc input;
    input.rootIndex = m_input_counter;
    input.type = InputType::BUFFER;

    auto& st = spaces[space];
    auto k = (int)input.type;
    uint32_t base = st.next[k];
    input.typeIndex = base;
    input.visibility = visibility;
    input.spaceIndex = space;

    m_impl->AddCBuffer(base, m_impl->GetVisibility(visibility), space);

    m_input_counter++;
    st.next[k]++;

    for (const auto& name : names)
    {
        m_descriptors[name] = input;
    }
    return *this;
}

KS::ShaderInputBlueprintBuilder& KS::ShaderInputBlueprintBuilder::AddStorageBuffer(ShaderInputVisibility visibility, int numberOfElements,
                                                                   std::string name, ShaderInputMod modifiable, uint32_t space)
{
    KS::ShaderInputDesc input;
    input.modifications = modifiable;
    input.numberOfElements = numberOfElements;
    input.rootIndex = m_input_counter;
    input.type = modifiable == ShaderInputMod::READ_ONLY ? InputType::RO_DATA : InputType::RW_DATA;

    auto& st = spaces[space];
    auto k = (int)input.type;
    uint32_t base = st.next[k];
    input.typeIndex = base;

    input.visibility = visibility;
    input.spaceIndex = space;

    m_descriptors[name] = input;

    m_impl->AddTable(
        m_impl->GetVisibility(visibility),
        modifiable == ShaderInputMod::READ_ONLY ? D3D12_DESCRIPTOR_RANGE_TYPE_SRV : D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, base,
        space);

    m_input_counter++;
    st.next[k]++;

    return *this;
}

KS::ShaderInputBlueprintBuilder& KS::ShaderInputBlueprintBuilder::AddTexture(ShaderInputVisibility visibility, std::string name,
                                                                             ShaderInputMod modifiable, uint32_t space)
{
    KS::ShaderInputDesc input;
    input.modifications = modifiable;
    input.rootIndex = m_input_counter;
    input.type = modifiable == ShaderInputMod::READ_ONLY ? InputType::RO_DATA : InputType::RW_DATA;

    auto& st = spaces[space];
    auto k = (int)input.type;
    uint32_t base = st.next[k];
    input.typeIndex = base;

    input.visibility = visibility;
    input.spaceIndex = space;

    m_descriptors[name] = input;

    m_impl->AddTable(
    m_impl->GetVisibility(visibility),
    modifiable == ShaderInputMod::READ_ONLY ? D3D12_DESCRIPTOR_RANGE_TYPE_SRV : D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, base,
    space);

    m_input_counter++;
    st.next[k]++;

    return *this;
}

KS::ShaderInputBlueprintBuilder& KS::ShaderInputBlueprintBuilder::AddStaticSampler(ShaderInputVisibility visibility,
                                                                                   SamplerDesc samplerDesc, uint32_t space)
{
    std::pair<ShaderInputVisibility, SamplerDesc> sampler = {visibility, samplerDesc};
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

    auto& st = spaces[space];
    auto k = (int)InputType::SAMPLER;
    uint32_t base = st.next[k];

    m_impl->AddSampler(base, m_impl->GetVisibility(sampler.first), addressMode, filterMode, borderColor,
                       D3D12_COMPARISON_FUNC_NEVER, space);
    m_sampler_inputs.push_back(sampler);
    st.next[k]++;

    return *this;
}

std::shared_ptr<KS::ShaderInputBlueprint> KS::ShaderInputBlueprintBuilder::Build(const Device& device, std::string name)
{
    wchar_t wString[4096];
    MultiByteToWideChar(CP_ACP, 0, name.c_str(), -1, wString, 4096);

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(static_cast<UINT>(m_impl->mParameters.size()), m_impl->mParameters.data(),
                           static_cast<UINT>(m_impl->mSamplers.size()), m_impl->mSamplers.data(),
        m_local ? D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE :
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                                          D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                                          D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                                          D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);

    ComPtr<ID3DBlob> serializedSignature;
    ComPtr<ID3DBlob> errBlob;

    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedSignature, &errBlob);

    if (FAILED(hr))
    {
        LOG(Log::Severity::WARN, "{}", (const char*)errBlob->GetBufferPointer());
        ASSERT(false && "Failed to serialize root signature");
    }
    auto engineDevice = reinterpret_cast<ID3D12Device5*>(device.GetDevice());

    ComPtr<ID3D12RootSignature> signature;
    hr = engineDevice->CreateRootSignature(0, serializedSignature->GetBufferPointer(), serializedSignature->GetBufferSize(),
                                           IID_PPV_ARGS(&signature));

    if (FAILED(hr))
    {
        MessageBox(NULL, L"Failed to initialize root signature", L"FATAL ERROR!", MB_ICONERROR | MB_OK);
        ASSERT(false && "Failed to initialize root signature");
    }
    signature->SetName(wString);

    return std::make_shared<ShaderInputBlueprint>(device, std::move(m_descriptors), signature.Get(), !m_local, name);
}

D3D12_SHADER_VISIBILITY KS::ShaderInputBlueprintBuilder::Impl::GetVisibility(ShaderInputVisibility visibility)
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
