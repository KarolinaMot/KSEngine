#include <FileIO.hpp>
#include <resources/Mesh.hpp>

#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>

void Mesh::AddAttribute(const std::string& name, ByteBuffer&& data)
{
    attribute_data.emplace(name, std::move(data));
}

const ByteBuffer* Mesh::GetAttribute(const std::string& name) const
{
    if (auto it = attribute_data.find(name); it != attribute_data.end())
    {
        return &it->second;
    }
    return nullptr;
}

void Mesh::save(BinarySaver& ar, const uint32_t v) const
{
    switch (v)
    {
    case 0:
        ar(cereal::make_nvp("Attributes", attribute_data));
        break;

    default:
        break;
    }
}

void Mesh::load(BinaryLoader& ar, const uint32_t v)
{
    switch (v)
    {
    case 0:
        ar(cereal::make_nvp("Attributes", attribute_data));
        break;

    default:
        break;
    }
}

std::optional<Mesh> MeshUtility::LoadMeshFromFile(const std::string& path)
{
    auto stream = FileIO::OpenReadStream(path);

    if (!stream)
        return std::nullopt;

    BinaryLoader loader { stream.value() };

    Mesh out {};
    loader(out);
    return out;
}