#pragma once
#include <renderer/DX12/Helpers/DXIncludes.hpp>
#include <DirectXMath.h>
#include <renderer/InfoStructs.hpp>

namespace KS
{
namespace Conversion
{

static std::wstring utf8_to_wide(const std::string& s)
{
    if (s.empty()) return std::wstring();
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), len);
    return w;
}

static DXGI_FORMAT KSFormatsToDXGI(KS::Formats format)
{
    switch (format)
    {
        case R8G8B8A8_UNORM:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
            break;
        case R8G8B8A8_UNORM_SRGB:
            return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
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
        case R32_TYPELESS:
            return DXGI_FORMAT_R32_TYPELESS;
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

static DirectX::XMMATRIX GLMToXMMATRIX(const glm::mat4& glmMatrix)
{
    // Create a DirectX::XMMATRIX from glm::mat4 by copying the data
    return DirectX::XMMATRIX(glmMatrix[0][0], glmMatrix[0][1], glmMatrix[0][2], glmMatrix[0][3], glmMatrix[1][0],
                             glmMatrix[1][1], glmMatrix[1][2], glmMatrix[1][3], glmMatrix[2][0], glmMatrix[2][1],
                             glmMatrix[2][2], glmMatrix[2][3], glmMatrix[3][0], glmMatrix[3][1], glmMatrix[3][2],
                             glmMatrix[3][3]);
}
}  // namespace Conversion

}  // namespace KS
