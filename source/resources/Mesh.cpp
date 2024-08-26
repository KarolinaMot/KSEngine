#include "Mesh.hpp"
#include <device/Device.hpp>

void KS::MeshData::AddAttribute(const std::string& name, ByteBuffer&& data)
{
    attribute_data.emplace(name, std::move(data));
}

const KS::ByteBuffer* KS::MeshData::GetAttribute(const std::string& name) const
{
    if (auto it = attribute_data.find(name); it != attribute_data.end())
    {
        return &it->second;
    }
    return nullptr;
}

KS::Mesh::Mesh(const Device& device, const MeshData& data)
{
    for (const auto& [name, attributes] : data)
    {
        auto view = attributes.GetView<uint8_t>();

        auto* start = view.begin();
        size_t size = view.count();
        size_t stride = MeshConstants::ATTRIBUTE_STRIDES.find(name)->second;

        ASSERT(size % stride == 0 && "Attribute stride is not divisible by provided data");

        auto buffer = std::make_shared<KS::StorageBuffer>(
            device, name, start, stride, size / stride, false);

        m_data.emplace(name, buffer);
    }
}

std::shared_ptr<KS::StorageBuffer> KS::Mesh::GetAttribute(const std::string& name) const
{
    if (auto it = m_data.find(name); it != m_data.end())
    {
        return it->second;
    }
    return nullptr;
}
