//
// Created by superhuap on 2026/11/18.
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

std::unique_ptr<IRenderBackend> CreateBackend()
{
#if defined(BACKEND_DX9)
    return std::make_unique<DX9_Backend>();
#elif defined(BACKEND_DX10)
    return std::make_unique<DX10_Backend>();
#elif defined(BACKEND_DX11)
    return std::make_unique<DX11_Backend>();
#elif defined(BACKEND_DX12)
    return std::make_unique<DX12_Backend>();
#elif defined(BACKEND_OPENGL)
    return std::make_unique<OPENGL_Backend>();
#elif defined(BACKEND_VULKAN)
    return std::make_unique<VULKAN_Backend>();
#else
    return nullptr;
#endif
}
