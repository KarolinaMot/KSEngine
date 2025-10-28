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
    //Mesh(Device& device, DXCommandList& commandList, const MeshData& data, const char* name, int meshIndex);
    Mesh(const char* name, uint32_t vOffset, uint32_t vCount, uint32_t iOffset, uint32_t iCount, uint32_t meshIndex,
         std::shared_ptr<DXResource> bLAS);
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    Mesh(Mesh&& other) noexcept;

    Mesh& operator=(Mesh&& other) noexcept;

    //std::shared_ptr<StorageBuffer> GetAttribute(const std::string& name) const;
    uint32_t BLASAddress() const;
    std::shared_ptr<DXResource> GetBLASRes() const;
    std::string GetName() const { return m_name; }
    uint32_t GetMeshIndex() const { return m_meshIndex; }
    uint32_t GetICount() const { return m_iCount; }
    uint32_t GetVCount() const { return m_vCount; }
    uint32_t GetVDataOffset() const { return m_vOffset; }
    uint32_t GetIDataOffset() const { return m_iOffset; }
    //void SetVDataOffset(uint32_t offset) { m_vDataOffset = offset; }
    //void SetIndexDataOffset(uint32_t offset) { m_indexDataOffset = offset; }

    private:
   // void BuildBLAS(const Device& device, DXCommandList& cmd);

    //std::unordered_map<std::string, std::shared_ptr<StorageBuffer>> m_data;
    std::string m_name;
    uint32_t m_meshIndex = 0;
    uint32_t m_vCount = 0, m_iCount = 0;
    uint32_t m_vOffset = 0;
    uint32_t m_iOffset = 0;

    class Impl;
    std::unique_ptr<Impl> m_impl;
};
}

CEREAL_CLASS_VERSION(KS::MeshData, 0);