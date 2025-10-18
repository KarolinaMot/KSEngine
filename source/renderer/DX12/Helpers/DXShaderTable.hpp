#pragma once
#include <vector>

#include "DXIncludes.hpp"
#include "DXResource.hpp"

class DXShaderTable
{
    struct TableRecord
    {
        std::wstring exportName;         // export or hit-group name
        std::vector<uint8_t> localArgs;  // serialized local-root arguments (optional)
    };

public:
    void AddRayGen(const std::wstring& exportName, const void* localData = nullptr, UINT localSize = 0);
    void AddMiss(const std::wstring& exportName, const void* localData = nullptr, UINT localSize = 0);
    void AddHitGroup(const std::wstring& exportName, const void* localData = nullptr, UINT localSize = 0);
    void Build(const ComPtr<ID3D12Device5>& device, ID3D12StateObjectProperties* props);
    void Clear();

    D3D12_DISPATCH_RAYS_DESC FillDispatchDesc(UINT width, UINT height, UINT depth = 1) const;

private:
    TableRecord MakeRecord(const std::wstring& name, const void* data, UINT size);
    UINT MaxRecordStride(const std::vector<TableRecord>& list, const auto& recSize) const;
    UINT MaxRecordStride(const TableRecord& record, const auto& recSize) const;
    void WriteTable(uint8_t* dst, const std::vector<TableRecord>& list, UINT stride, ID3D12StateObjectProperties* props);
    void WriteTable(uint8_t* dst, const TableRecord& list, ID3D12StateObjectProperties* props);
    inline UINT Align(UINT v, UINT a) { return (v + a - 1u) & ~(a - 1u); }
    inline UINT64 Align64(UINT64 v, UINT64 a) { return (v + a - 1ull) & ~(a - 1ull); }

private:
    TableRecord m_raygen;
    std::vector<TableRecord> m_miss;
    std::vector<TableRecord> m_hit;

    std::unique_ptr<DXResource> m_upl;
    UINT64 m_gpuVA = 0, m_size = 0;

    // Per-table stride (>= 32B) and offsets (64B aligned)
    UINT m_strideRG = 0, m_strideMS = 0, m_strideHG = 0, m_strideCB = 0;
    UINT64 m_offRG = 0, m_offMS = 0, m_offHG = 0, m_offCB = 0;
};
