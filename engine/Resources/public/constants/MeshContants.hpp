#pragma once

#include <Common.hpp>
#include <cstdint>

namespace MeshConstants
{
enum AttributesIndices : size_t
{
    INDICES,
    POSITIONS,
    NORMALS,
    TEXTURE_UVS,
    TANGENTS,
    BITANGENTS,

    ATTRIBUTE_COUNT
};

constexpr const char* AttributeNames[] {
    "INDICES",
    "POSITIONS",
    "NORMALS",
    "UVS",
    "TANGENTS",
    "BITANGENTS"
};

constexpr size_t AttributeStrides[] {
    sizeof(uint32_t),
    sizeof(float) * 3,
    sizeof(float) * 3,
    sizeof(float) * 2,
    sizeof(float) * 3,
    sizeof(float) * 3
};
}