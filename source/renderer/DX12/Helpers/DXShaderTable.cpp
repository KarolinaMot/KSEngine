#include "DXShaderTable.h"
#include <code_utility.hpp>

void DXShaderTable::AddRayGen(const std::wstring& exportName, const void* localData, UINT localSize)
{
    m_raygen = MakeRecord(exportName, localData, localSize);
}

void DXShaderTable::AddMiss(const std::wstring& exportName, const void* localData, UINT localSize)
{
    m_miss.push_back(MakeRecord(exportName, localData, localSize));
}

void DXShaderTable::AddHitGroup(const std::wstring& exportName, const void* localData, UINT localSize)
{
    m_hit.push_back(MakeRecord(exportName, localData, localSize));
}

void DXShaderTable::Build(const ComPtr<ID3D12Device5>& device, ID3D12StateObjectProperties* props)
{ 
     auto recSize = [&](const TableRecord& r)
     {
            return Align(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + UINT(r.localArgs.size()),
                         D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);  // 32
     };

     m_strideRG = MaxRecordStride(m_raygen, recSize);
     m_strideMS = MaxRecordStride(m_miss, recSize);
     m_strideHG = MaxRecordStride(m_hit, recSize);

     const UINT countRG = 1;  // usually 1
     const UINT countMS = (UINT)m_miss.size();
     const UINT countHG = (UINT)m_hit.size();

     UINT64 sizeRG = m_strideRG * countRG;
     UINT64 sizeMS = m_strideMS * countMS;
     UINT64 sizeHG = m_strideHG * countHG;

     UINT64 cursor = 0;

     // RayGen (always present: 1 record)
     m_offRG = Align64(cursor, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
     cursor = m_offRG + sizeRG;

     // Miss (optional)
     if (countMS)
     {
         m_offMS = Align64(cursor, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
         cursor = m_offMS + sizeMS;
     }
     else
     {
         m_offMS = 0;
     }

     // Hit (optional)
     if (countHG)
     {
         m_offHG = Align64(cursor, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
         cursor = m_offHG + sizeHG;
     }
     else
     {
         m_offHG = 0;
     }

     m_size = cursor;  // final end; no fallbacks needed
     m_size = cursor ? cursor : sizeRG;

     m_upl = std::make_unique<DXResource>(device,
         CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
         CD3DX12_RESOURCE_DESC::Buffer(m_size, D3D12_RESOURCE_FLAG_NONE),
         nullptr,
         "Shader table buffer");

     uint8_t* base = nullptr;

     HRESULT hr = m_upl->GetResource()->Map(0, nullptr, (void**)&base);
     if (FAILED(hr)) ASSERT(false && "Failed to map shader table");

     WriteTable(base + m_offRG, m_raygen, props);
     if (countMS) WriteTable(base + m_offMS, m_miss, m_strideMS, props);
     if (countHG) WriteTable(base + m_offHG, m_hit, m_strideHG, props);

     m_upl->GetResource()->Unmap(0, nullptr);
     m_gpuVA = m_upl->GetResource()->GetGPUVirtualAddress();
}


void DXShaderTable::Clear()
{
    m_raygen = TableRecord();
    m_miss.clear();
    m_hit.clear();
    m_gpuVA = 0;
    m_size = 0;
    m_strideRG = m_strideMS = m_strideHG = m_strideCB = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;
    m_offRG = m_offMS = m_offHG = m_offCB = 0;
}

D3D12_DISPATCH_RAYS_DESC DXShaderTable::FillDispatchDesc(UINT width, UINT height, UINT depth) const
{
    D3D12_DISPATCH_RAYS_DESC out{};

    out.RayGenerationShaderRecord.StartAddress = m_gpuVA + m_offRG;
    out.RayGenerationShaderRecord.SizeInBytes = m_strideRG;  // one record

    // Miss
    if (!m_miss.empty())
    {
        out.MissShaderTable.StartAddress = m_gpuVA + m_offMS;
        out.MissShaderTable.StrideInBytes = m_strideMS;
        out.MissShaderTable.SizeInBytes = m_strideMS * (UINT)m_miss.size();
    }

    // Hit groups
    if (!m_hit.empty())
    {
        out.HitGroupTable.StartAddress = m_gpuVA + m_offHG;
        out.HitGroupTable.StrideInBytes = m_strideHG;
        out.HitGroupTable.SizeInBytes = m_strideHG * (UINT)m_hit.size();
    }

    out.Width = width;
    out.Height = height;
    out.Depth = depth;

    return out;
}

DXShaderTable::TableRecord DXShaderTable::MakeRecord(const std::wstring& name, const void* data, UINT size)
{ 
    TableRecord r;
    r.exportName = name;
    if (data && size)
    {
        r.localArgs.resize(size);
        memcpy(r.localArgs.data(), data, size);
    }
    return r;
}

UINT DXShaderTable::MaxRecordStride(const std::vector<TableRecord>& list, const auto& recSize) const
{
    UINT m = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;  // at least 32
    for (auto& r : list) m = std::max(m, recSize(r));
    return m;
}

UINT DXShaderTable::MaxRecordStride(const TableRecord& record, const auto& recSize) const 
{ 
    UINT m = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;  // at least 32
    m = std::max(m, recSize(record));
    return m;
}

void DXShaderTable::WriteTable(uint8_t* base, const std::vector<TableRecord>& list, UINT stride,
                               ID3D12StateObjectProperties* props)
{
    for (size_t i = 0; i < list.size(); ++i)
    {
        uint8_t* dst = base + i * stride;
        const auto& r = list[i];

        // id
        const void* id = props->GetShaderIdentifier(r.exportName.c_str());
        memcpy(dst, id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

        // locals
        if (!r.localArgs.empty())
        {
            memcpy(dst + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, r.localArgs.data(), r.localArgs.size());
        }

        // pad the rest of the stride with zeros (important)
        size_t used = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + r.localArgs.size();
        if (used < stride) memset(dst + used, 0, stride - used);
    }
}

void DXShaderTable::WriteTable(uint8_t* dst, const TableRecord& list, ID3D12StateObjectProperties* props)
{
    const void* id = props->GetShaderIdentifier(list.exportName.c_str());
    assert(id && "Export name not found in this pipeline.");
    // Write identifier
    memcpy(dst, id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    // Write local-root arguments (if any) immediately after the ID
    if (!list.localArgs.empty())
    {
        memcpy(dst + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, list.localArgs.data(), list.localArgs.size());
    }
}
