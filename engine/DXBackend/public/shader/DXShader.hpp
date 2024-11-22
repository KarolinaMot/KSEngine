#pragma once

#include <Common.hpp>
#include <DXCommon.hpp>

#include <dxcapi.h>

struct DXShader
{
    enum class Type
    {
        COMPUTE,
        VERTEX,
        PIXEL
    };

    Type type {};
    ComPtr<IDxcBlob> blob {};
};