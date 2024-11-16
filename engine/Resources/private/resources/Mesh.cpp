#include <resources/Mesh.hpp>

void MeshData::AddAttribute(const std::string& name, ByteBuffer&& data)
{
    attribute_data.emplace(name, std::move(data));
}

const ByteBuffer* MeshData::GetAttribute(const std::string& name) const
{
    if (auto it = attribute_data.find(name); it != attribute_data.end())
    {
        return &it->second;
    }
    return nullptr;
}
