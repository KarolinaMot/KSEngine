#include "Mesh.hpp"

void KS::Mesh::AddAttribute(const std::string& name, ByteBuffer&& data)
{
    attribute_data.emplace(name, std::move(data));
}

const KS::ByteBuffer* KS::Mesh::GetAttribute(const std::string& name) const
{
    if (auto it = attribute_data.find(name); it != attribute_data.end())
    {
        return &it->second;
    }
    return nullptr;
}
