#pragma once

#include <Common.hpp>
#include <gpu_resources/DXResource.hpp>

struct Mesh
{
    DXResource position {};
    DXResource normals {};
    DXResource uvs {};
    DXResource tangents {};
    DXResource indices {};
    size_t index_count {};
};