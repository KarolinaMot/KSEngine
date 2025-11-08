#include "../ShaderInputBlueprint.hpp"
#include "Helpers/DXIncludes.hpp"
#include "device/Device.hpp"
#include "tools/Log.hpp"
#include <renderer/ShaderInputBlueprintBuilder.hpp>

class KS::ShaderInputBlueprint::Impl
{
public:
    ComPtr<ID3D12RootSignature> m_signature;
};


KS::ShaderInputBlueprint::ShaderInputBlueprint(std::unordered_map<std::string, ShaderInputDesc>&& inputs, void* signature,
                               bool global, std::string name)
{
    m_impl = std::make_unique<Impl>();
    m_descriptors = std::move(inputs);
    m_isGlobal = global;
    auto* rs = static_cast<ID3D12RootSignature*>(signature);
    m_impl->m_signature = rs;
}

KS::ShaderInputBlueprint::~ShaderInputBlueprint()
{
}

void* KS::ShaderInputBlueprint::GetSignature() const
{
    return m_impl->m_signature.Get();
}

KS::ShaderInputDesc KS::ShaderInputBlueprint::GetInput(std::string key) const
{
    auto res = m_descriptors.find(key);
    if (res == m_descriptors.end())
    {
        LOG(Log::Severity::FATAL, "Key was not found");
        throw;
    }
    return res->second;
}
