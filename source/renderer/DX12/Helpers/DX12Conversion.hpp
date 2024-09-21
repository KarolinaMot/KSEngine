#pragma once
#include <renderer/InfoStructs.hpp>

namespace KS
{
static DXGI_FORMAT KSFormatsToDXGI(KS::Formats format)
{
    switch (format)
    {
    case R8G8B8A8_UNORM:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
        break;
    case R16G16B16A16_FLOAT:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
        break;
    case R32G32B32A32_FLOAT:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
        break;
    case R16_FLOAT:
        return DXGI_FORMAT_R16_FLOAT;
        break;
    case R32_FLOAT:
        return DXGI_FORMAT_R32_FLOAT;
        break;
    case D32_FLOAT:
        return DXGI_FORMAT_D32_FLOAT;
        break;
    default:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
        break;
    }
};

static Formats DXGIFormatsToKS(DXGI_FORMAT format)
{
    switch (format)
    {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
        return R8G8B8A8_UNORM;
        break;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
        return R16G16B16A16_FLOAT;
        break;
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
        return R32G32B32A32_FLOAT;
        break;
    case DXGI_FORMAT_R16_FLOAT:
        return R16_FLOAT;
        break;
    case DXGI_FORMAT_R32_FLOAT:
        return R32_FLOAT;
        break;
    case DXGI_FORMAT_D32_FLOAT:
        return D32_FLOAT;
        break;
    default:
        return R8G8B8A8_UNORM;
        break;
    }
}

} // namespace KS
