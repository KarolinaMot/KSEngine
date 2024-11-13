#pragma once

#include <Common.hpp>
#include <DXCommon.hpp>

#include <deque>
#include <optional>
#include <unordered_map>
#include <vector>

class DXShaderInputs
{
public:
    DXShaderInputs() = default;

    DXShaderInputs(ComPtr<ID3D12RootSignature> root_signature, const std::unordered_map<std::string, uint32_t>& input_table)
        : root_signature(root_signature)
        , input_table(input_table)
    {
    }

    NON_COPYABLE(DXShaderInputs);

    DXShaderInputs(DXShaderInputs&&) = default;
    DXShaderInputs& operator=(DXShaderInputs&&) = default;

    ID3D12RootSignature* GetSignature() { return root_signature.Get(); }

    std::optional<uint32_t> GetInputIndex(const std::string& name)
    {
        if (auto it = input_table.find(name); it != input_table.end())
        {
            return it->second;
        }
        return std::nullopt;
    }

private:
    ComPtr<ID3D12RootSignature> root_signature {};
    std::unordered_map<std::string, uint32_t> input_table {};
};

// TODO: Can be superseeded by shader reflection
class DXShaderInputsBuilder
{
    using Self = DXShaderInputsBuilder;

public:
    DXShaderInputsBuilder() = default;

    Self& AddRootConstant(
        const std::string& name,
        uint32_t shader_register,
        uint32_t size_in_32_bits,
        D3D12_SHADER_VISIBILITY visibility);

    Self& AddStorageBuffer(
        const std::string& name,
        uint32_t shader_register,
        D3D12_ROOT_PARAMETER_TYPE buffer_type,
        D3D12_SHADER_VISIBILITY visibility);

    Self& AddDescriptorTable(
        const std::string& name,
        uint32_t shader_register,
        uint32_t num_descriptors,
        D3D12_DESCRIPTOR_RANGE_TYPE rangeType,
        D3D12_SHADER_VISIBILITY visibility);

    struct StaticSamplerParameters
    {
        D3D12_STATIC_BORDER_COLOR border_color = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        D3D12_TEXTURE_ADDRESS_MODE address_mode = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        D3D12_COMPARISON_FUNC comparison = D3D12_COMPARISON_FUNC_NEVER;
        D3D12_FILTER filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    };

    Self& AddStaticSampler(
        const std::string& name,
        uint32_t shader_register,
        const StaticSamplerParameters& sampler_desc,
        D3D12_SHADER_VISIBILITY visibility);

    static inline constexpr auto DEFAULT_INPUT_NAME = L"Root Signature";
    static inline constexpr auto DEFAULT_INPUT_FLAGS = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    std::optional<DXShaderInputs> Build(ID3D12Device* device, const wchar_t* name = DEFAULT_INPUT_NAME, const D3D12_ROOT_SIGNATURE_FLAGS& flags = DEFAULT_INPUT_FLAGS) const;

private:
    void AddNameIndexMapping(const std::string& name, uint32_t index);
    std::unordered_map<std::string, uint32_t> name_index_map {};

    // Deque used to not invalidate pointers as resizing occurs
    std::deque<D3D12_DESCRIPTOR_RANGE1> descriptor_ranges;

    std::vector<D3D12_ROOT_PARAMETER1> root_parameters;
    std::vector<D3D12_STATIC_SAMPLER_DESC> static_samplers;
};