#pragma once
#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>
#include <containers/ByteBuffer.hpp>
#include <map>
#include <memory>
#include <renderer/StorageBuffer.hpp>

class DXResource;
class DXCommandList;
namespace KS
{

namespace MeshConstants
{

    const std::string ATTRIBUTE_INDICES_NAME = "INDICES";
    const std::string ATTRIBUTE_POSITIONS_NAME = "POSITIONS";
    const std::string ATTRIBUTE_NORMALS_NAME = "NORMALS";
    const std::string ATTRIBUTE_TEXTURE_UVS_NAME = "UVS";
    const std::string ATTRIBUTE_TANGENTS_NAME = "TANGENTS";
    const std::string ATTRIBUTE_BITANGENTS_NAME = "BITANGENTS";

    const std::string ATTRIBUTE_ARRAY[6] = {ATTRIBUTE_INDICES_NAME, ATTRIBUTE_POSITIONS_NAME, ATTRIBUTE_NORMALS_NAME,
                                      ATTRIBUTE_TEXTURE_UVS_NAME, ATTRIBUTE_TANGENTS_NAME,  ATTRIBUTE_BITANGENTS_NAME};

    const std::unordered_map<std::string, size_t> ATTRIBUTE_STRIDES {
        { ATTRIBUTE_INDICES_NAME, sizeof(uint32_t) },
        { ATTRIBUTE_POSITIONS_NAME, sizeof(float) * 3 },
        { ATTRIBUTE_NORMALS_NAME, sizeof(float) * 3 },
        { ATTRIBUTE_TEXTURE_UVS_NAME, sizeof(float) * 2 },
        { ATTRIBUTE_TANGENTS_NAME, sizeof(float) * 3 },
        { ATTRIBUTE_BITANGENTS_NAME, sizeof(float) * 3 }
    };

}

class MeshData
{
public:
    MeshData() = default;
    void AddAttribute(const std::string& name, ByteBuffer&& data);
    const ByteBuffer* GetAttribute(const std::string& name) const;

    auto begin() const { return attribute_data.begin(); }
    auto end() const { return attribute_data.end(); }


private:
    friend class ::cereal::access;

    template <typename A>
    void save(A& ar, const uint32_t v) const;

    template <typename A>
    void load(A& ar, const uint32_t v);

    std::map<std::string, ByteBuffer> attribute_data;
};
template <typename A>
inline void MeshData::save(A& ar, const uint32_t v) const
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
inline void MeshData::load(A& ar, const uint32_t v)
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

class Device;
class Mesh
{
public:
    Mesh(Device& device, DXCommandList& commandList, const MeshData& data, const char* name, int meshIndex);
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    Mesh(Mesh&& other) noexcept;

    Mesh& operator=(Mesh&& other) noexcept;

    std::shared_ptr<StorageBuffer> GetAttribute(const std::string& name) const;
    uint32_t BLASAddress() const;
    std::shared_ptr<DXResource> GetBLASRes() const;
    std::string GetName() const { return m_name; }
    int GetMeshIndex() const { return m_meshIndex; }
    int GetICount() const { return m_iCount; }
    int GetVCount() const { return m_vCount; }
    int GetVDataOffset() const { return m_vDataOffset; }
    int GetIDataOffset() const { return m_indexDataOffset; }
    void SetVDataOffset(uint32_t offset) { m_vDataOffset = offset; }
    void SetIndexDataOffset(uint32_t offset) { m_indexDataOffset = offset; }

    private:
    void BuildBLAS(const Device& device, DXCommandList& cmd);

    std::unordered_map<std::string, std::shared_ptr<StorageBuffer>> m_data;
    std::string m_name;
    int m_meshIndex = 0;
    uint32_t m_vCount = 0, m_iCount = 0;
    uint32_t m_vDataOffset = 0;
    uint32_t m_indexDataOffset = 0;

    class Impl;
    std::unique_ptr<Impl> m_impl;
};
}

CEREAL_CLASS_VERSION(KS::MeshData, 0);