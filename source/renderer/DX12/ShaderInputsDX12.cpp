#include "../ShaderInputCollection.hpp"
#include "Helpers/DXIncludes.hpp"
#include "Helpers/DXSignature.hpp"
#include "device/Device.hpp"
#include "tools/Log.hpp"
#include <renderer/ShaderInputCollectionBuilder.hpp>

class KS::ShaderInputCollection::Impl
{
public:
    ComPtr<ID3D12RootSignature> m_signature;
};

//KS::ShaderInputCollection::ShaderInputCollection(const Device& device, std::unordered_map<std::string, ShaderInputDesc>&& inputs, const std::vector<std::pair<ShaderInputVisibility, SamplerDesc>>& samplers, int totalDataCount, std::string name)
//{
//    m_impl = std::make_unique<Impl>();
//    DXSignatureBuilder builder = DXSignatureBuilder(totalDataCount);
//    m_descriptors = std::move(inputs);
//    int descriptorCounter = 0;
//    int srvCounter = 0;
//    int uavCounter = 0;
//    int cbvCounter = 0;
//
//    for (auto& pair : m_descriptors)
//    {
//        D3D12_SHADER_VISIBILITY visibility;
//        switch (pair.second.visibility[0])
//        {
//        case ShaderInputVisibility::PIXEL:
//            visibility = D3D12_SHADER_VISIBILITY_PIXEL;
//            break;
//        case ShaderInputVisibility::VERTEX:
//            visibility = D3D12_SHADER_VISIBILITY_VERTEX;
//            break;
//        case ShaderInputVisibility::COMPUTE:
//            visibility = D3D12_SHADER_VISIBILITY_ALL;
//            break;
//        }
//
//        switch (pair.second.type)
//        {
//        case InputType::BUFFER:
//            pair.second.typeIndex = cbvCounter;
//            cbvCounter++;
//            builder.AddCBuffer(pair.second.typeIndex, visibility);
//            break;
//        case InputType::RO_DATA:
//            pair.second.typeIndex = srvCounter;
//            builder.AddTable(visibility, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, pair.second.numberOfElements[0], srvCounter);
//            srvCounter++;
//            break;
//        case InputType::RW_DATA:
//            pair.second.typeIndex = uavCounter;
//            uavCounter++;
//            builder.AddTable(visibility, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, pair.second.numberOfElements[0],
//                             pair.second.typeIndex);
//            break;
//        case InputType::RANGE:
//            std::vector<std::tuple<D3D12_DESCRIPTOR_RANGE_TYPE, int, int>> ranges;
//            for (int i = 0; i < pair.second.numOfRanges; i++)
//            {
//                D3D12_DESCRIPTOR_RANGE_TYPE type;
//                int typeIndex = 0;
//                if (pair.second.modifications[i] == ShaderInputMod::READ_ONLY)
//                {
//                    type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
//                    typeIndex = srvCounter;
//                    srvCounter++;
//                }
//                else
//                {
//                    type = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
//                    typeIndex = uavCounter;
//                    uavCounter++;
//                }
//                ranges.push_back(std::tuple<D3D12_DESCRIPTOR_RANGE_TYPE, int, int>(type, pair.second.numberOfElements[i], typeIndex));
//            }
//            builder.AddRanges(visibility, ranges);
//        }
//
//        pair.second.rootIndex = descriptorCounter;
//        descriptorCounter++;
//    }
//
//    for (int i = 0; i < samplers.size(); i++)
//    {
//        D3D12_SHADER_VISIBILITY visibility;
//        switch (samplers[i].first)
//        {
//        case ShaderInputVisibility::PIXEL:
//            visibility = D3D12_SHADER_VISIBILITY_PIXEL;
//            break;
//        case ShaderInputVisibility::VERTEX:
//            visibility = D3D12_SHADER_VISIBILITY_VERTEX;
//            break;
//        case ShaderInputVisibility::COMPUTE:
//            visibility = D3D12_SHADER_VISIBILITY_ALL;
//            break;
//        }
//
//        D3D12_TEXTURE_ADDRESS_MODE addressMode;
//        switch (samplers[i].second.addressMode)
//        {
//        case SamplerAddressMode::SAM_CLAMP:
//            addressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
//            break;
//        case SamplerAddressMode::SAM_MIRROR:
//            addressMode = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
//            break;
//        case SamplerAddressMode::SAM_BORDER:
//            addressMode = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
//            break;
//        case SamplerAddressMode::SAM_MIRROR_ONCE:
//            addressMode = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
//            break;
//        case SamplerAddressMode::SAM_WRAP:
//            addressMode = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
//            break;
//        }
//
//        D3D12_FILTER filterMode{};
//        switch (samplers[i].second.filter)
//        {
//        case SamplerFilter::SF_NEAREST:
//            filterMode = D3D12_FILTER_MIN_MAG_MIP_POINT;
//            break;
//
//        case SamplerFilter::SF_LINEAR:
//            filterMode = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
//            break;
//
//        case SamplerFilter::SF_ANISOTROPIC:
//            filterMode = D3D12_FILTER_ANISOTROPIC;
//            break;
//        }
//
//        D3D12_STATIC_BORDER_COLOR borderColor{};
//        switch (samplers[i].second.borderColor)
//        {
//        case SamplerBorderColor::SBC_TRANSPARENT_BLACK:
//            borderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
//            break;
//        case SamplerBorderColor::SBC_OPAQUE_BLACK:
//            borderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
//            break;
//        case SamplerBorderColor::SBC_OPAQUE_WHITE:
//            borderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
//            break;
//        }
//
//        builder.AddSampler(i, visibility, addressMode, filterMode, borderColor);
//    }
//
//    wchar_t wString[4096];
//    MultiByteToWideChar(CP_ACP, 0, name.c_str(), -1, wString, 4096);
//
//    m_impl->m_signature = builder.Build(reinterpret_cast<ID3D12Device5*>(device.GetDevice()), wString);
//}

KS::ShaderInputCollection::ShaderInputCollection(const Device& device, std::unordered_map<std::string, ShaderInputDesc>&& inputs, void* signature, std::string name)
{
    m_impl = std::make_unique<Impl>();
    m_descriptors = std::move(inputs);

    m_impl->m_signature = reinterpret_cast<ID3D12RootSignature*>(signature);
}

KS::ShaderInputCollection::~ShaderInputCollection()
{
}

void* KS::ShaderInputCollection::GetSignature() const
{
    return m_impl->m_signature.Get();
}

KS::ShaderInputDesc KS::ShaderInputCollection::GetInput(std::string key) const
{
    auto res = m_descriptors.find(key);
    if (res == m_descriptors.end())
    {
        LOG(Log::Severity::FATAL, "Key was not found");
        throw;
    }
    return res->second;
}
