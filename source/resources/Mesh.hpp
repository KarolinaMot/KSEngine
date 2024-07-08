#pragma once
#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>
#include <containers/ByteBuffer.hpp>
#include <map>

namespace KS
{

const std::string ATTRIBUTE_INDICES_NAME = "INDICES";
const std::string ATTRIBUTE_POSITIONS_NAME = "POSITIONS";
const std::string ATTRIBUTE_NORMALS_NAME = "NORMALS";
const std::string ATTRIBUTE_TEXTURE_UVS_NAME = "UVS";
const std::string ATTRIBUTE_TANGENTS_NAME = "TANGENTS";
const std::string ATTRIBUTE_BITANGENTS_NAME = "BITANGENTS";

class Mesh
{
public:
    Mesh() = default;
    void AddAttribute(const std::string& name, ByteBuffer&& data);
    const ByteBuffer* GetAttribute(const std::string& name) const;

private:
    friend class ::cereal::access;

    template <typename A>
    void save(A& ar, const uint32_t v) const;

    template <typename A>
    void load(A& ar, const uint32_t v);

    std::map<std::string, ByteBuffer> attribute_data;
};
template <typename A>
inline void Mesh::save(A& ar, const uint32_t v) const
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
template <typename A>
inline void Mesh::load(A& ar, const uint32_t v)
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
}

CEREAL_CLASS_VERSION(KS::Mesh, 0);