#pragma once

// WINDOWS STUFF
#define NOMINMAX

#include <Windows.h>
#include <wrl.h>
using namespace Microsoft::WRL;

#include <directx/d3d12.h>

// DirectX 12 specific headers.
#include <d3dcompiler.h>
#include <dxgi1_6.h>

#include <directx/d3dx12.h>

#define FRAME_BUFFER_COUNT 2

inline void CheckDX(HRESULT result)
{
    if (FAILED(result))
    {
        throw;
    }
}