#include "../ShaderInputCollection.hpp"
#include "Helpers/DXIncludes.hpp"
#include "device/Device.hpp"
#include "tools/Log.hpp"
#include <renderer/ShaderInputCollectionBuilder.hpp>

class KS::ShaderInputCollection::Impl
{
public:
    ComPtr<ID3D12RootSignature> m_signature;
};

KS::ShaderInputCollection::ShaderInputCollection(std::unordered_map<std::string, ShaderInputDesc>&& inputs, void* signature, std::string name)
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
