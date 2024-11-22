#include <Log.hpp>
#include <shader/DXShaderInputs.hpp>

std::optional<DXShaderInputs> DXShaderInputsBuilder::Build(ID3D12Device* device, const wchar_t* name, const D3D12_ROOT_SIGNATURE_FLAGS& flags) const
{
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc;

    desc.Init_1_1(
        root_parameters.size(),
        root_parameters.data(),
        static_samplers.size(),
        static_samplers.data(),
        flags);

    ComPtr<ID3DBlob> serializedSignature;
    ComPtr<ID3DBlob> errBlob;

    HRESULT hr = D3D12SerializeVersionedRootSignature(
        &desc,
        &serializedSignature,
        &errBlob);

    if (FAILED(hr))
    {
        Log("Shader Input Builder Error: {}", (char*)errBlob->GetBufferPointer());
        return std::nullopt;
    }

    ComPtr<ID3D12RootSignature> signature;

    CheckDX(device->CreateRootSignature(
        0,
        serializedSignature->GetBufferPointer(),
        serializedSignature->GetBufferSize(),
        IID_PPV_ARGS(&signature)));

    signature->SetName(name);
    return DXShaderInputs { signature, name_index_map };
}

DXShaderInputsBuilder& DXShaderInputsBuilder::AddRootConstant(const std::string& name, uint32_t shaderRegister, uint32_t size_in_32_bits, D3D12_SHADER_VISIBILITY shader)
{
    AddNameIndexMapping(name);

    D3D12_ROOT_PARAMETER1 par = {};
    par.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    par.Constants.Num32BitValues = size_in_32_bits;
    par.Constants.RegisterSpace = 0;
    par.Constants.ShaderRegister = shaderRegister;
    par.ShaderVisibility = shader;

    root_parameters.push_back(par);
    return *this;
}

DXShaderInputsBuilder& DXShaderInputsBuilder::AddRootDescriptor(const std::string& name, uint32_t shaderRegister, D3D12_ROOT_PARAMETER_TYPE buffer_type, D3D12_SHADER_VISIBILITY shader)
{
    AddNameIndexMapping(name);

    D3D12_ROOT_DESCRIPTOR1 desc {};
    desc.RegisterSpace = 0;
    desc.ShaderRegister = shaderRegister;
    desc.Flags = {};

    D3D12_ROOT_PARAMETER1 par {};
    par.ParameterType = buffer_type;
    par.Descriptor = desc;
    par.ShaderVisibility = shader;

    root_parameters.push_back(par);
    return *this;
}

DXShaderInputsBuilder& DXShaderInputsBuilder::AddDescriptorTable(const std::string& name, uint32_t shader_register, uint32_t num_descriptors, D3D12_DESCRIPTOR_RANGE_TYPE rangeType, D3D12_SHADER_VISIBILITY shader)
{
    AddNameIndexMapping(name);

    D3D12_DESCRIPTOR_RANGE1 range {};
    range.NumDescriptors = num_descriptors;
    range.BaseShaderRegister = shader_register;
    range.RangeType = rangeType;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    range.RegisterSpace = 0;
    range.Flags = {};

    auto& emplaced_range = descriptor_ranges.emplace_back(range);

    D3D12_ROOT_DESCRIPTOR_TABLE1 descriptorTable {};
    descriptorTable.NumDescriptorRanges = 1;
    descriptorTable.pDescriptorRanges = &emplaced_range;

    D3D12_ROOT_PARAMETER1 par {};
    par.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    par.DescriptorTable = descriptorTable;
    par.ShaderVisibility = shader;

    root_parameters.push_back(par);
    return *this;
}

DXShaderInputsBuilder& DXShaderInputsBuilder::AddStaticSampler(const std::string& name, uint32_t shaderRegister, const StaticSamplerParameters& sampler_desc, D3D12_SHADER_VISIBILITY shader)
{
    AddNameIndexMapping(name);

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = sampler_desc.filter;
    sampler.AddressU = sampler_desc.address_mode;
    sampler.AddressV = sampler_desc.address_mode;
    sampler.AddressW = sampler_desc.address_mode;
    sampler.ComparisonFunc = sampler_desc.comparison;
    sampler.BorderColor = sampler_desc.border_color;

    sampler.ShaderVisibility = shader;
    sampler.ShaderRegister = shaderRegister;
    sampler.RegisterSpace = 0;

    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 0;

    static_samplers.push_back(sampler);
    return *this;
}

void DXShaderInputsBuilder::AddNameIndexMapping(const std::string& name)
{
    if (name_index_map.contains(name))
    {
        Log("Shader Input Builder Warning: {} is already an existing parameter name, this will break name-index mappings", name);
    }

    name_index_map[name] = index_counter++;
}