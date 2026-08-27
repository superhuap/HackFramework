//
// Created by superhuap on 2026/8/25.
//

#include "backend/DX9_Backend.h"
#include "backend/MinHookHelper.h"

#include <d3d9.h>

#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>

#include "menu/Menu.h"
#include "utils/InputHook.h"
#include "utils/Logger.h"

namespace
{

    LPDIRECT3D9 g_d3d = nullptr;
    LPDIRECT3DDEVICE9 g_device = nullptr;

    void CleanupDeviceD3D9()
    {
        if (g_d3d)
        {
            g_d3d->Release();
            g_d3d = nullptr;
        }
        if (g_device)
        {
            g_device->Release();
            g_device = nullptr;
        }
    }

    bool CreateDeviceD3D9(HWND hWnd)
    {
        g_d3d = Direct3DCreate9(D3D_SDK_VERSION);
        if (!g_d3d)
        {
            LOG_ERROR("Direct3DCreate9() failed");
            return false;
        }

        D3DPRESENT_PARAMETERS d3dpp = {};
        d3dpp.Windowed = TRUE;
        d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;

        const HRESULT hr = g_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, hWnd,
                                               D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &g_device);
        if (hr != D3D_OK)
        {
            LOG_ERROR("CreateDevice() failed. [rv: {}]", static_cast<unsigned long>(hr));
            return false;
        }

        return true;
    }

    HRESULT(WINAPI * oReset)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*) = nullptr;
    HRESULT WINAPI Hook_Reset(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters)
    {
        ImGui_ImplDX9_InvalidateDeviceObjects();
        return oReset(pDevice, pPresentationParameters);
    }

    HRESULT(WINAPI * oResetEx)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*, D3DDISPLAYMODEEX*) = nullptr;
    HRESULT WINAPI Hook_ResetEx(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters,
                                D3DDISPLAYMODEEX* pFullscreenDisplayMode)
    {
        ImGui_ImplDX9_InvalidateDeviceObjects();
        return oResetEx(pDevice, pPresentationParameters, pFullscreenDisplayMode);
    }

    void RenderImGui_D3D9(IDirect3DDevice9* pDevice);

    HRESULT(WINAPI * oPresent)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*) = nullptr;
    HRESULT WINAPI Hook_Present(IDirect3DDevice9* pDevice, const RECT* pSourceRect, const RECT* pDestRect,
                                HWND hDestWindowOverride, const RGNDATA* pDirtyRegion)
    {
        RenderImGui_D3D9(pDevice);
        return oPresent(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
    }

    HRESULT(WINAPI * oPresentEx)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*, DWORD) = nullptr;
    HRESULT WINAPI Hook_PresentEx(IDirect3DDevice9* pDevice, const RECT* pSourceRect, const RECT* pDestRect,
                                  HWND hDestWindowOverride, const RGNDATA* pDirtyRegion, DWORD dwFlags)
    {
        RenderImGui_D3D9(pDevice);
        return oPresentEx(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
    }

    void RenderImGui_D3D9(IDirect3DDevice9* pDevice)
    {
        if (!ImGui::GetIO().BackendRendererUserData)
            ImGui_ImplDX9_Init(pDevice);

        if (Utils::Input::shutting_down.load() || !ImGui::GetCurrentContext())
            return;

        DWORD srgbWriteEnable = 0;
        pDevice->GetRenderState(D3DRS_SRGBWRITEENABLE, &srgbWriteEnable);
        pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);

        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        menu::Menu::GetInstance().Render();

        ImGui::EndFrame();
        if (pDevice->BeginScene() == D3D_OK)
        {
            ImGui::Render();
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            pDevice->EndScene();
        }

        pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, srgbWriteEnable);
    }

} // namespace

bool DX9_Backend::Initialize(HWND hWnd)
{
    if (!menu::Menu::GetInstance().Initialize(hWnd))
        return false;

    if (!CreateDeviceD3D9(GetConsoleWindow()))
    {
        LOG_ERROR("CreateDeviceD3D9() failed");
        return false;
    }

    LOG_INFO("DirectX9: g_pD3D: {}", reinterpret_cast<void*>(g_d3d));
    LOG_INFO("DirectX9: g_pd3dDevice: {}", reinterpret_cast<void*>(g_device));

    void** vtable = *reinterpret_cast<void***>(g_device);

    void* fnReset = vtable[16];
    void* fnResetEx = vtable[132];
    void* fnPresent = vtable[17];
    void* fnPresentEx = vtable[121];

    CleanupDeviceD3D9();

    bool ok = true;
    ok &= backend::CreateHookOnce(fnReset, &Hook_Reset, reinterpret_cast<void**>(&oReset), "Reset") != nullptr;
    ok &= backend::CreateHookOnce(fnResetEx, &Hook_ResetEx, reinterpret_cast<void**>(&oResetEx), "ResetEx") != nullptr;
    ok &= backend::CreateHookOnce(fnPresent, &Hook_Present, reinterpret_cast<void**>(&oPresent), "Present") != nullptr;
    ok &= backend::CreateHookOnce(fnPresentEx, &Hook_PresentEx, reinterpret_cast<void**>(&oPresentEx), "PresentEx") != nullptr;

    ok &= backend::EnableHook(fnReset, "Reset");
    ok &= backend::EnableHook(fnResetEx, "ResetEx");
    ok &= backend::EnableHook(fnPresent, "Present");
    ok &= backend::EnableHook(fnPresentEx, "PresentEx");

    if (!ok)
        return false;

    LOG_INFO("DirectX9 Backend initialized successfully");
    return true;
}

void DX9_Backend::Shutdown()
{
    LOG_INFO("DirectX9 Backend shutting down...");

    if (ImGui::GetCurrentContext())
    {
        if (ImGui::GetIO().BackendRendererUserData)
            ImGui_ImplDX9_Shutdown();
    }

    menu::Menu::GetInstance().Shutdown();
    CleanupDeviceD3D9();

    LOG_INFO("DirectX9 Backend shutdown complete");
}
