//
// Created by superhuap on 2026/8/25.
//

#include "backend/DX11_Backend.h"
#include "backend/MinHookHelper.h"

#include <d3d11.h>
#include <dxgi1_2.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include "menu/Menu.h"
#include "utils/InputHook.h"
#include "utils/Logger.h"

namespace
{

    ID3D11Device* g_device = nullptr;
    ID3D11DeviceContext* g_deviceContext = nullptr;
    ID3D11RenderTargetView* g_renderTarget = nullptr;
    IDXGISwapChain* g_swapChain = nullptr;

    void CleanupRenderTarget()
    {
        if (g_renderTarget)
        {
            g_renderTarget->Release();
            g_renderTarget = nullptr;
        }
    }

    void CleanupDeviceD3D11()
    {
        CleanupRenderTarget();

        if (g_swapChain)
        {
            g_swapChain->Release();
            g_swapChain = nullptr;
        }
        if (g_device)
        {
            g_device->Release();
            g_device = nullptr;
        }
        if (g_deviceContext)
        {
            g_deviceContext->Release();
            g_deviceContext = nullptr;
        }
    }

    bool CreateDeviceD3D11(HWND hWnd)
    {
        DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
        swapChainDesc.Windowed = TRUE;
        swapChainDesc.BufferCount = 2;
        swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.OutputWindow = hWnd;
        swapChainDesc.SampleDesc.Count = 1;

        const D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_0,
        };

        const HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_NULL, nullptr, 0, featureLevels, 2,
                                                         D3D11_SDK_VERSION, &swapChainDesc, &g_swapChain, &g_device,
                                                         nullptr, nullptr);
        if (hr != S_OK)
        {
            LOG_ERROR("D3D11CreateDeviceAndSwapChain() failed. [rv: {}]", static_cast<unsigned long>(hr));
            return false;
        }

        return true;
    }

    void CreateRenderTarget(IDXGISwapChain* pSwapChain)
    {
        ID3D11Texture2D* pBackBuffer = nullptr;
        if (SUCCEEDED(pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer))))
        {
            DXGI_SWAP_CHAIN_DESC sd;
            pSwapChain->GetDesc(&sd);

            D3D11_RENDER_TARGET_VIEW_DESC desc = {};
            desc.Format = sd.BufferDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
                                      ? DXGI_FORMAT_R8G8B8A8_UNORM
                                      : sd.BufferDesc.Format;
            desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

            g_device->CreateRenderTargetView(pBackBuffer, &desc, &g_renderTarget);
            pBackBuffer->Release();
        }
    }

    void RenderImGui_DX11(IDXGISwapChain* pSwapChain)
    {
        if (Utils::Input::shutting_down.load())
            return;

        if (!ImGui::GetIO().BackendRendererUserData)
        {
            if (SUCCEEDED(pSwapChain->GetDevice(IID_PPV_ARGS(&g_device))))
                g_device->GetImmediateContext(&g_deviceContext);
            if (g_device && g_deviceContext)
                ImGui_ImplDX11_Init(g_device, g_deviceContext);
        }

        if (!g_renderTarget)
            CreateRenderTarget(pSwapChain);

        if (ImGui::GetCurrentContext() && g_device && g_deviceContext && g_renderTarget)
        {
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            Menu::Render();

            ImGui::Render();

            g_deviceContext->OMSetRenderTargets(1, &g_renderTarget, nullptr);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        }
    }

    HRESULT(WINAPI * oPresent)(IDXGISwapChain*, UINT, UINT) = nullptr;
    HRESULT WINAPI Hook_Present(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
    {
        RenderImGui_DX11(pSwapChain);
        return oPresent(pSwapChain, SyncInterval, Flags);
    }

    HRESULT(WINAPI * oPresent1)(IDXGISwapChain*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*) = nullptr;
    HRESULT WINAPI Hook_Present1(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT PresentFlags,
                                 const DXGI_PRESENT_PARAMETERS* pPresentParameters)
    {
        RenderImGui_DX11(pSwapChain);
        return oPresent1(pSwapChain, SyncInterval, PresentFlags, pPresentParameters);
    }

    HRESULT(WINAPI * oResizeBuffers)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT) = nullptr;
    HRESULT WINAPI Hook_ResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height,
                                      DXGI_FORMAT NewFormat, UINT SwapChainFlags)
    {
        CleanupRenderTarget();
        return oResizeBuffers(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
    }

    HRESULT(WINAPI * oResizeBuffers1)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT, const UINT*,
                                      IUnknown* const*) = nullptr;
    HRESULT WINAPI Hook_ResizeBuffers1(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height,
                                       DXGI_FORMAT NewFormat, UINT SwapChainFlags, const UINT* pCreationNodeMask,
                                       IUnknown* const* ppPresentQueue)
    {
        CleanupRenderTarget();
        return oResizeBuffers1(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags,
                               pCreationNodeMask, ppPresentQueue);
    }

    HRESULT(WINAPI * oCreateSwapChain)(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**) = nullptr;
    HRESULT WINAPI Hook_CreateSwapChain(IDXGIFactory* pFactory, IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc,
                                        IDXGISwapChain** ppSwapChain)
    {
        CleanupRenderTarget();
        return oCreateSwapChain(pFactory, pDevice, pDesc, ppSwapChain);
    }

    HRESULT(WINAPI * oCreateSwapChainForHwnd)(IDXGIFactory*, IUnknown*, HWND, const DXGI_SWAP_CHAIN_DESC1*,
                                              const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*,
                                              IDXGISwapChain1**) = nullptr;
    HRESULT WINAPI Hook_CreateSwapChainForHwnd(IDXGIFactory* pFactory, IUnknown* pDevice, HWND hWnd,
                                               const DXGI_SWAP_CHAIN_DESC1* pDesc,
                                               const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,
                                               IDXGIOutput* pRestrictOutput, IDXGISwapChain1** ppSwapChain)
    {
        CleanupRenderTarget();
        return oCreateSwapChainForHwnd(pFactory, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictOutput, ppSwapChain);
    }

    HRESULT(WINAPI * oCreateSwapChainForCoreWindow)(IDXGIFactory*, IUnknown*, IUnknown*,
                                                    const DXGI_SWAP_CHAIN_DESC1*, IDXGIOutput*,
                                                    IDXGISwapChain1**) = nullptr;
    HRESULT WINAPI Hook_CreateSwapChainForCoreWindow(IDXGIFactory* pFactory, IUnknown* pDevice, IUnknown* pWindow,
                                                     const DXGI_SWAP_CHAIN_DESC1* pDesc, IDXGIOutput* pRestrictOutput,
                                                     IDXGISwapChain1** ppSwapChain)
    {
        CleanupRenderTarget();
        return oCreateSwapChainForCoreWindow(pFactory, pDevice, pWindow, pDesc, pRestrictOutput, ppSwapChain);
    }

    HRESULT(WINAPI * oCreateSwapChainForComposition)(IDXGIFactory*, IUnknown*, const DXGI_SWAP_CHAIN_DESC1*,
                                                     IDXGIOutput*, IDXGISwapChain1**) = nullptr;
    HRESULT WINAPI Hook_CreateSwapChainForComposition(IDXGIFactory* pFactory, IUnknown* pDevice,
                                                      const DXGI_SWAP_CHAIN_DESC1* pDesc, IDXGIOutput* pRestrictOutput,
                                                      IDXGISwapChain1** ppSwapChain)
    {
        CleanupRenderTarget();
        return oCreateSwapChainForComposition(pFactory, pDevice, pDesc, pRestrictOutput, ppSwapChain);
    }

} // namespace

bool DX11_Backend::Initialize(HWND hWnd)
{
    if (!Menu::Initialize(hWnd))
        return false;

    if (!CreateDeviceD3D11(GetConsoleWindow()))
    {
        LOG_ERROR("CreateDeviceD3D11() failed");
        return false;
    }

    LOG_INFO("DirectX11: g_pd3dDevice: {}", reinterpret_cast<void*>(g_device));
    LOG_INFO("DirectX11: g_pSwapChain: {}", reinterpret_cast<void*>(g_swapChain));

    IDXGIDevice* dxgiDevice = nullptr;
    IDXGIAdapter* dxgiAdapter = nullptr;
    IDXGIFactory* factory = nullptr;

    g_device->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
    if (dxgiDevice)
        dxgiDevice->GetAdapter(&dxgiAdapter);
    if (dxgiAdapter)
        dxgiAdapter->GetParent(IID_PPV_ARGS(&factory));

    if (!factory)
    {
        LOG_ERROR("pIDXGIFactory is NULL.");
        if (dxgiAdapter) dxgiAdapter->Release();
        if (dxgiDevice) dxgiDevice->Release();
        CleanupDeviceD3D11();
        return false;
    }

    void** vtable = *reinterpret_cast<void***>(g_swapChain);
    void** factoryVtable = *reinterpret_cast<void***>(factory);

    void* fnCreateSwapChain = factoryVtable[10];
    void* fnCreateSwapChainForHwnd = factoryVtable[15];
    void* fnCreateSwapChainForCoreWindow = factoryVtable[16];
    void* fnCreateSwapChainForComposition = factoryVtable[24];

    void* fnPresent = vtable[8];
    void* fnPresent1 = vtable[22];
    void* fnResizeBuffers = vtable[13];
    void* fnResizeBuffers1 = vtable[39];

    factory->Release();
    dxgiAdapter->Release();
    dxgiDevice->Release();

    CleanupDeviceD3D11();

    bool ok = true;
    ok &= backend::CreateHookOnce(fnCreateSwapChain, &Hook_CreateSwapChain,
                                  reinterpret_cast<void**>(&oCreateSwapChain), "CreateSwapChain") != nullptr;
    ok &= backend::CreateHookOnce(fnCreateSwapChainForHwnd, &Hook_CreateSwapChainForHwnd,
                                  reinterpret_cast<void**>(&oCreateSwapChainForHwnd), "CreateSwapChainForHwnd") != nullptr;
    ok &= backend::CreateHookOnce(fnCreateSwapChainForCoreWindow, &Hook_CreateSwapChainForCoreWindow,
                                  reinterpret_cast<void**>(&oCreateSwapChainForCoreWindow),
                                  "CreateSwapChainForCoreWindow") != nullptr;
    ok &= backend::CreateHookOnce(fnCreateSwapChainForComposition, &Hook_CreateSwapChainForComposition,
                                  reinterpret_cast<void**>(&oCreateSwapChainForComposition),
                                  "CreateSwapChainForComposition") != nullptr;

    ok &= backend::CreateHookOnce(fnPresent, &Hook_Present, reinterpret_cast<void**>(&oPresent), "Present") != nullptr;
    ok &= backend::CreateHookOnce(fnPresent1, &Hook_Present1, reinterpret_cast<void**>(&oPresent1), "Present1") != nullptr;

    ok &= backend::CreateHookOnce(fnResizeBuffers, &Hook_ResizeBuffers,
                                  reinterpret_cast<void**>(&oResizeBuffers), "ResizeBuffers") != nullptr;
    ok &= backend::CreateHookOnce(fnResizeBuffers1, &Hook_ResizeBuffers1,
                                  reinterpret_cast<void**>(&oResizeBuffers1), "ResizeBuffers1") != nullptr;

    ok &= backend::EnableHook(fnCreateSwapChain, "CreateSwapChain");
    ok &= backend::EnableHook(fnCreateSwapChainForHwnd, "CreateSwapChainForHwnd");
    ok &= backend::EnableHook(fnCreateSwapChainForCoreWindow, "CreateSwapChainForCoreWindow");
    ok &= backend::EnableHook(fnCreateSwapChainForComposition, "CreateSwapChainForComposition");

    ok &= backend::EnableHook(fnPresent, "Present");
    ok &= backend::EnableHook(fnPresent1, "Present1");

    ok &= backend::EnableHook(fnResizeBuffers, "ResizeBuffers");
    ok &= backend::EnableHook(fnResizeBuffers1, "ResizeBuffers1");

    if (!ok)
        return false;

    LOG_INFO("DirectX11 Backend initialized successfully");
    return true;
}

void DX11_Backend::Shutdown()
{
    LOG_INFO("DirectX11 Backend shutting down...");

    if (ImGui::GetCurrentContext())
    {
        if (ImGui::GetIO().BackendRendererUserData)
            ImGui_ImplDX11_Shutdown();
    }

    Menu::Shutdown();
    CleanupDeviceD3D11();

    LOG_INFO("DirectX11 Backend shutdown complete");
}
