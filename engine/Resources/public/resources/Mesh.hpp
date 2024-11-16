#pragma once

#include <Common.hpp>
#include <SerializationCommon.hpp>

#include <map>
#include <optional>
#include <resources/ByteBuffer.hpp>

namespace MeshConstants
{

constexpr auto ATTRIBUTE_INDICES_NAME = "INDICES";
constexpr auto ATTRIBUTE_POSITIONS_NAME = "POSITIONS";
constexpr auto ATTRIBUTE_NORMALS_NAME = "NORMALS";
constexpr auto ATTRIBUTE_TEXTURE_UVS_NAME = "UVS";
constexpr auto ATTRIBUTE_TANGENTS_NAME = "TANGENTS";
constexpr auto ATTRIBUTE_BITANGENTS_NAME = "BITANGENTS";

const std::unordered_map<std::string, size_t> ATTRIBUTE_STRIDES {
    { ATTRIBUTE_INDICES_NAME, sizeof(uint32_t) },
    { ATTRIBUTE_POSITIONS_NAME, sizeof(float) * 3 },
    { ATTRIBUTE_NORMALS_NAME, sizeof(float) * 3 },
    { ATTRIBUTE_TEXTURE_UVS_NAME, sizeof(float) * 2 },
    { ATTRIBUTE_TANGENTS_NAME, sizeof(float) * 3 },
    { ATTRIBUTE_BITANGENTS_NAME, sizeof(float) * 3 }
};

}

class Mesh
{
public:
    Mesh() = default;

    void AddAttribute(const std::string& name, ByteBuffer&& data);
    const ByteBuffer* GetAttribute(const std::string& name) const;

private:
    std::map<std::string, ByteBuffer> attribute_data;

    friend class cereal::access;
    void save(BinarySaver& ar, const uint32_t v) const;
    void load(BinaryLoader& ar, const uint32_t v);
};

CEREAL_CLASS_VERSION(Mesh, 0);

namespace MeshUtility
{
std::optional<Mesh> LoadMeshFromFile(const std::string& path);
}