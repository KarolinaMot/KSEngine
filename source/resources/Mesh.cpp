#include "Mesh.hpp"
#include <device/Device.hpp>
#include <renderer/DX12/Helpers/DX12Common.hpp>
#include <renderer/DX12/Helpers/DXCommandList.hpp>

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
    DXCommandList* commandList = reinterpret_cast<DXCommandList*>(device.GetCommandList());
    m_name = data.m_name;
    for (const auto& [name, attributes] : data)
    {
        auto view = attributes.GetView<uint8_t>();

        auto* start = view.begin();
        size_t size = view.count();
        size_t stride = MeshConstants::ATTRIBUTE_STRIDES.find(name)->second;

        ASSERT(size % stride == 0 && "Attribute stride is not divisible by provided data");

        StorageBuffer::StorageBufferFlags flag = StorageBuffer::StorageBufferFlags::VERTEX_DATA_BUFFER;
        if (name == MeshConstants::ATTRIBUTE_INDICES_NAME) 
            flag = StorageBuffer::StorageBufferFlags::INDEX_DATA_BUFFER;


        auto buffer =
            std::make_shared<KS::StorageBuffer>(device, *commandList, name, start, stride, size / stride, false, flag);

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
