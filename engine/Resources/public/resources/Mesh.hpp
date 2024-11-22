#pragma once

#include <Common.hpp>
#include <SerializationCommon.hpp>

#include <map>
#include <optional>
#include <resources/ByteBuffer.hpp>

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