//
// Created by superhuap on 2026/8/25.
//

#include "backend/BackendFactory.h"

#if defined(BACKEND_DX9)
#include "backend/DX9_Backend.h"
#elif defined(BACKEND_DX10)
#include "backend/DX10_Backend.h"
#elif defined(BACKEND_DX11)
#include "backend/DX11_Backend.h"
#elif defined(BACKEND_DX12)
#include "backend/DX12_Backend.h"
#elif defined(BACKEND_OPENGL)
#include "backend/OPENGL_Backend.h"
#elif defined(BACKEND_VULKAN)
#include "backend/VULKAN_Backend.h"
#endif

const char* GetBackendName()
{
#if defined(BACKEND_DX9)
    return "DIRECTX9";
#elif defined(BACKEND_DX10)
    return "DIRECTX10";
#elif defined(BACKEND_DX11)
    return "DIRECTX11";
#elif defined(BACKEND_DX12)
    return "DIRECTX12";
#elif defined(BACKEND_OPENGL)
    return "OPENGL";
#elif defined(BACKEND_VULKAN)
    return "VULKAN";
#else
    return "NONE";
#endif
}

IRenderBackend* CreateBackend()
{
#if defined(BACKEND_DX9)
    return new DX9_Backend();
#elif defined(BACKEND_DX10)
    return new DX10_Backend();
#elif defined(BACKEND_DX11)
    return new DX11_Backend();
#elif defined(BACKEND_DX12)
    return new DX12_Backend();
#elif defined(BACKEND_OPENGL)
    return new OPENGL_Backend();
#elif defined(BACKEND_VULKAN)
    return new VULKAN_Backend();
#else
    return nullptr;
#endif
}
