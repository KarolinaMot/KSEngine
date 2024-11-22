#pragma once

#include <Windows.h>
#include <wrl.h>

#include <directx/d3d12.h>
#include <directx/d3dx12.h>

using Microsoft::WRL::ComPtr;
constexpr uint32_t FRAME_BUFFER_COUNT = 2;

inline void CheckDX(HRESULT result)
{
    if (FAILED(result))
    {
        throw;
    }
}