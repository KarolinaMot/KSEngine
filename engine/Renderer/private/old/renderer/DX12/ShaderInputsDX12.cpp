#include "../ShaderInputs.hpp"
#include "Device.hpp"
#include "Helpers/DXIncludes.hpp"
#include "Helpers/DXSignature.hpp"
#include "Log.hpp"
#include <renderer/ShaderInputsBuilder.hpp>

class ShaderInputs::Impl
{
public:
    ComPtr<ID3D12RootSignature> m_signature;
};

ShaderInputs::ShaderInputs(const Device& device, std::unordered_map<std::string, ShaderInput>&& inputs, const std::vector<std::pair<ShaderInputVisibility, SamplerDesc>>& samplers, int totalDataCount, std::string name)
{
    m_impl = std::make_unique<Impl>();
    DXSignatureBuilder builder = DXSignatureBuilder(totalDataCount);
    m_descriptors = std::move(inputs);
    int descriptorCounter = 0;
    int srvCounter = 0;
    int uavCounter = 0;
    int cbvCounter = 0;

    for (auto& pair : m_descriptors)
    {
        D3D12_SHADER_VISIBILITY visibility;
        switch (pair.second.visibility)
        {
        case ShaderInputVisibility::PIXEL:
            visibility = D3D12_SHADER_VISIBILITY_PIXEL;
            break;
        case ShaderInputVisibility::VERTEX:
            visibility = D3D12_SHADER_VISIBILITY_VERTEX;
            break;
        case ShaderInputVisibility::COMPUTE:
            visibility = D3D12_SHADER_VISIBILITY_ALL;
            break;
        }

        switch (pair.second.type)
        {
        case InputType::BUFFER:
            pair.second.typeIndex = cbvCounter;
            cbvCounter++;
            builder.AddCBuffer(pair.second.typeIndex, visibility);
            break;
        case InputType::RO_DATA:
            pair.second.typeIndex = srvCounter;
            builder.AddTable(visibility, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, pair.second.numberOfElements, srvCounter);
            srvCounter++;
            break;
        case InputType::RW_DATA:
            pair.second.typeIndex = uavCounter;
            uavCounter++;
            builder.AddTable(visibility, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, pair.second.numberOfElements, pair.second.typeIndex);
            break;
        }

        pair.second.rootIndex = descriptorCounter;
        descriptorCounter++;
    }

    for (int i = 0; i < samplers.size(); i++)
    {
        D3D12_SHADER_VISIBILITY visibility;
        switch (samplers[i].first)
        {
        case ShaderInputVisibility::PIXEL:
            visibility = D3D12_SHADER_VISIBILITY_PIXEL;
            break;
        case ShaderInputVisibility::VERTEX:
            visibility = D3D12_SHADER_VISIBILITY_VERTEX;
            break;
        case ShaderInputVisibility::COMPUTE:
            visibility = D3D12_SHADER_VISIBILITY_ALL;
            break;
        }

        D3D12_TEXTURE_ADDRESS_MODE addressMode;
        switch (samplers[i].second.addressMode)
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
        switch (samplers[i].second.filter)
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
        switch (samplers[i].second.borderColor)
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

        builder.AddSampler(i, visibility, addressMode, filterMode, borderColor);
    }

    wchar_t wString[4096];
    MultiByteToWideChar(CP_ACP, 0, name.c_str(), -1, wString, 4096);

    m_impl->m_signature = builder.Build(reinterpret_cast<ID3D12Device5*>(device.GetDevice()), wString);
}

ShaderInputs::ShaderInputs(const Device& device, std::unordered_map<std::string, ShaderInput>&& inputs, void* signature, std::string name)
{
    m_impl = std::make_unique<Impl>();
    m_descriptors = std::move(inputs);

    m_impl->m_signature = reinterpret_cast<ID3D12RootSignature*>(signature);
}

ShaderInputs::~ShaderInputs()
{
}

void* ShaderInputs::GetSignature() const
{
    return m_impl->m_signature.Get();
}

ShaderInput ShaderInputs::GetInput(std::string key) const
{
    auto res = m_descriptors.find(key);
    if (res == m_descriptors.end())
    {
        Log("Key was not found");
        throw;
    }
    return res->second;
}
