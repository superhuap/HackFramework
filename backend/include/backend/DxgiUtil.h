//
// Created by superhuap on 2026/11/18.
// Adapted from Universal-Dear-ImGui-Hook (MIT), src/windows/dxgi.cpp.
// Header-only: shared by the DX10/DX11/DX12 backends (one DLL each).
//

#ifndef HACKFRAMEWORK_DXGIUTIL_H
#define HACKFRAMEWORK_DXGIUTIL_H

#include <windows.h>
#include <dxgi.h>

#include <mutex>

#include "core/WinUtil.h"

namespace Dxgi
{

    // Virtual method indices of IDXGISwapChain / IDXGISwapChain1 / IDXGISwapChain3.
    constexpr size_t kPresentIndex = 8;
    constexpr size_t kResizeBuffersIndex = 13;
    constexpr size_t kPresent1Index = 22;
    constexpr size_t kResizeBuffers1Index = 39;

    // The window of the swap chain. A swap chain created for composition has
    // none: find the window of the process with the size of the back buffer.
    inline HWND SwapChainWindow(IDXGISwapChain* swapChain, const DXGI_SWAP_CHAIN_DESC& desc)
    {
        if (desc.OutputWindow)
            return desc.OutputWindow;

        static std::mutex mutex;
        static IDXGISwapChain* known = nullptr;
        static HWND window = nullptr;
        std::lock_guard<std::mutex> lock(mutex);
        if (known != swapChain || !IsWindow(window))
        {
            known = swapChain;
            window = Core::FindProcessWindow(desc.BufferDesc.Width, desc.BufferDesc.Height);
        }
        return window;
    }

    // Format usable for a render target view on a swap chain buffer.
    inline DXGI_FORMAT RenderTargetFormat(DXGI_FORMAT format)
    {
        switch (format)
        {
        case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case DXGI_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_UNORM;
        case DXGI_FORMAT_B8G8R8X8_TYPELESS: return DXGI_FORMAT_B8G8R8X8_UNORM;
        case DXGI_FORMAT_R10G10B10A2_TYPELESS: return DXGI_FORMAT_R10G10B10A2_UNORM;
        case DXGI_FORMAT_R16G16B16A16_TYPELESS: return DXGI_FORMAT_R16G16B16A16_FLOAT;
        default: return format;
        }
    }

    // True for render target formats that store linear values: Dear ImGui
    // produces sRGB vertex colors, which must be converted before drawing.
    inline bool StoresLinear(DXGI_FORMAT format)
    {
        switch (format)
        {
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
            return true;
        default:
            return false;
        }
    }

} // namespace Dxgi

#endif // HACKFRAMEWORK_DXGIUTIL_H
