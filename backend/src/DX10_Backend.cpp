//
// Created by superhuap on 2026/11/18.
// Adapted from Universal-Dear-ImGui-Hook (MIT), src/windows/d3d10.cpp and dxgi.cpp.
//

#include "backend/DX10_Backend.h"

#include <d3d10_1.h>
#include <dxgi1_4.h>

#include <atomic>
#include <mutex>
#include <vector>

#include <imgui.h>
#include <imgui_impl_dx10.h>
#include <imgui_impl_win32.h>

#include "backend/DxgiUtil.h"
#include "backend/ImGuiColors.h"
#include "menu/Menu.h"
#include "utils/HookHelper.h"
#include "utils/InputHook.h"
#include "utils/InputLock.h"
#include "utils/Logger.h"

#include "core/Guarded.h"
#include "core/ScopedCall.h"
#include "core/Teardown.h"
#include "core/WinUtil.h"

namespace
{

    using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
    using Present1Fn = HRESULT(__stdcall*)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
    using ResizeBuffersFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
    using ResizeBuffers1Fn = HRESULT(__stdcall*)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT,
                                                 const UINT*, IUnknown* const*);
    using D3D10CreateDeviceAndSwapChainFn =
        HRESULT(__stdcall*)(IDXGIAdapter*, D3D10_DRIVER_TYPE, HMODULE, UINT, UINT,
                            const DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**, ID3D10Device**, D3D_FEATURE_LEVEL*,
                            ID3D10Device**);

    // The device and swap chain of the game, bound at its first Present.
    // Released by Teardown() when the device is replaced or removed.
    ID3D10Device* g_device = nullptr;
    IDXGISwapChain* g_swapChain = nullptr;
    HWND g_boundWindow = nullptr;

    PresentFn oPresent = nullptr;
    Present1Fn oPresent1 = nullptr;
    ResizeBuffersFn oResizeBuffers = nullptr;
    ResizeBuffers1Fn oResizeBuffers1 = nullptr;

    struct Hook
    {
        const char* name;
        void* target;
    };
    std::vector<Hook> g_installedHooks;

    // One overlay thread at a time, on the swap chain that claimed the process.
    std::mutex g_frameMutex;

    void Teardown();

    void LogProbeFailure()
    {
        static std::atomic<bool> g_logged{false};
        if (!g_logged.exchange(true))
            Logger::Debug("[DX10] Probe: not a D3D10 swap chain, ignoring it");
    }

    // Binds the overlay to the presenting swap chain: probes the device behind
    // it, creates or rebinds the ImGui context on its window and initializes
    // the renderer. False when the swap chain does not belong to a D3D10
    // device (a D3D11 game, say): the DLL keeps waiting for one that does.
    bool Init(IDXGISwapChain* swapChain)
    {
        DXGI_SWAP_CHAIN_DESC desc{};
        ID3D10Device* device = nullptr;
        if (FAILED(swapChain->GetDesc(&desc)) || FAILED(swapChain->GetDevice(IID_PPV_ARGS(&device))))
        {
            LogProbeFailure();
            return false;
        }

        // The application replaced its device: release the overlay first.
        if (g_device && device != g_device)
            Teardown();

        HWND window = Dxgi::SwapChainWindow(swapChain, desc);
        if (!Menu::Initialize(window))
        {
            Core::Release(device);
            return false;
        }

        if (!g_device)
        {
            g_device = device;
            ImGui_ImplDX10_Init(g_device);
        }
        else
        {
            Core::Release(device);
        }
        g_swapChain = swapChain;
        g_boundWindow = window;
        Logger::Debug("[DX10] Init: bound to swap chain 0x{:X}, window {:p}",
                      reinterpret_cast<unsigned long long>(swapChain), reinterpret_cast<void*>(window));
        return true;
    }

    // The render target view of the current back buffer, created per frame so
    // that a ResizeBuffers needs no bookkeeping. `linear` reports a render
    // target that stores linear values.
    ID3D10RenderTargetView* CreateTarget(bool& linear)
    {
        ID3D10Texture2D* buffer = nullptr;
        if (FAILED(g_swapChain->GetBuffer(0, IID_PPV_ARGS(&buffer))))
            return nullptr;
        D3D10_TEXTURE2D_DESC textureDesc{};
        buffer->GetDesc(&textureDesc);
        D3D10_RENDER_TARGET_VIEW_DESC viewDesc{};
        viewDesc.Format = Dxgi::RenderTargetFormat(textureDesc.Format);
        linear = Dxgi::StoresLinear(viewDesc.Format);
        viewDesc.ViewDimension = textureDesc.SampleDesc.Count > 1 ? D3D10_RTV_DIMENSION_TEXTURE2DMS
                                                                  : D3D10_RTV_DIMENSION_TEXTURE2D;
        ID3D10RenderTargetView* target = nullptr;
        HRESULT hr = g_device->CreateRenderTargetView(buffer, &viewDesc, &target);
        buffer->Release();
        if (FAILED(hr))
            Logger::Error("[DX10] Render: CreateRenderTargetView failed (0x{:08X})", static_cast<unsigned long>(hr));
        return target;
    }

    // ImGui frame: renderer new frame, menu between NewFrame() and Render()
    // under the input lock, then the draw data to render.
    ImDrawData* Frame()
    {
        InputLock::Scoped lock;
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        Menu::Render();

        ImGui::Render();
        return ImGui::GetDrawData();
    }

    void RenderFrame()
    {
        ImGui_ImplDX10_NewFrame();
        ImDrawData* drawData = Frame();

        bool linear = false;
        ID3D10RenderTargetView* target = CreateTarget(linear);
        if (!target)
            return;
        if (linear)
            ImGuiColors::LinearizeColors(drawData);

        // Save and restore the pipeline state the game left behind.
        ID3D10RenderTargetView* previous[D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
        ID3D10DepthStencilView* depth = nullptr;
        g_device->OMGetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, previous, &depth);
        g_device->OMSetRenderTargets(1, &target, nullptr);
        // A fault in the renderer must not leave the back buffer bound.
        Core::Guarded([&] { ImGui_ImplDX10_RenderDrawData(drawData); });
        g_device->OMSetRenderTargets(D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT, previous, depth);
        for (ID3D10RenderTargetView* view : previous)
            Core::Release(view);
        Core::Release(depth);
        Core::Release(target);
    }

    // Releases the renderer, the ImGui context and the device references.
    // Runs on the rendering thread, from ServiceTeardown() or after a device
    // removal; idempotent.
    void Teardown()
    {
        if (!g_device && !g_swapChain)
            return;
        Logger::Debug("[DX10] Teardown: releasing the overlay");
        if (ImGui::GetCurrentContext() && ImGui::GetIO().BackendRendererUserData)
            ImGui_ImplDX10_Shutdown();
        Menu::Shutdown();
        Core::Release(g_device);
        g_swapChain = nullptr;
        g_boundWindow = nullptr;
    }

    void OnPresent(IDXGISwapChain* swapChain, UINT flags)
    {
        if (flags & DXGI_PRESENT_TEST)
            return;
        if (Core::Faulted())
            return;
        // The window of the bound swap chain is gone: wait for the new one.
        if (g_boundWindow && !IsWindow(g_boundWindow))
            return;

        if (swapChain != g_swapChain && !Init(swapChain))
            return;

        // Several swap chains may present, from one thread or more: the first
        // one that presents here draws the overlay.
        if (!g_frameMutex.try_lock())
            return;
        Core::Guarded([] { RenderFrame(); });
        Core::ServiceTeardown(&Teardown);
        g_frameMutex.unlock();
    }

    // The application answers a removed device with a new one: release our
    // references before they see the result of the present.
    HRESULT AfterPresent(HRESULT result)
    {
        if (result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET)
        {
            static std::atomic<bool> g_logged{false};
            if (!g_logged.exchange(true))
                Logger::Error("[DX10] Present: device removed (0x{:08X}), releasing the overlay",
                              static_cast<unsigned long>(result));
            Core::Guarded(&Teardown);
        }
        else
        {
            return result;
        }
        return result;
    }

    HRESULT STDMETHODCALLTYPE HookPresent(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags)
    {
        Core::ScopedCall call;
        if (!InputHook::shutting_down.load())
            Core::Guarded([&] { OnPresent(swapChain, flags); });
        else
            Core::ServiceTeardown(&Teardown);
        return AfterPresent(oPresent(swapChain, syncInterval, flags));
    }

    HRESULT STDMETHODCALLTYPE HookPresent1(IDXGISwapChain1* swapChain, UINT syncInterval, UINT flags,
                                           const DXGI_PRESENT_PARAMETERS* params)
    {
        Core::ScopedCall call;
        if (!InputHook::shutting_down.load())
            Core::Guarded([&] { OnPresent(swapChain, flags); });
        else
            Core::ServiceTeardown(&Teardown);
        return AfterPresent(oPresent1(swapChain, syncInterval, flags, params));
    }

    HRESULT STDMETHODCALLTYPE HookResizeBuffers(IDXGISwapChain* swapChain, UINT bufferCount, UINT width, UINT height,
                                                DXGI_FORMAT newFormat, UINT flags)
    {
        Core::ScopedCall call;
        // The per-frame render target needs no pre-resize bookkeeping.
        return oResizeBuffers(swapChain, bufferCount, width, height, newFormat, flags);
    }

    HRESULT STDMETHODCALLTYPE HookResizeBuffers1(IDXGISwapChain3* swapChain, UINT bufferCount, UINT width, UINT height,
                                                 DXGI_FORMAT newFormat, UINT flags, const UINT* nodeMasks,
                                                 IUnknown* const* queues)
    {
        Core::ScopedCall call;
        return oResizeBuffers1(swapChain, bufferCount, width, height, newFormat, flags, nodeMasks, queues);
    }

    bool InstallHook(const char* name, void* target, void* detour, void** original)
    {
        if (!HookHelper::InstallHook(name, target, detour, original))
            return false;
        g_installedHooks.push_back({name, target});
        return true;
    }

} // namespace

bool DX10_Backend::Initialize()
{
    Logger::Debug("[DX10] Install: starting");

    Core::ResetFault();
    InputHook::shutting_down.store(false);

    // A dummy D3D10 swap chain provides the IDXGISwapChain vtable to hook.
    HMODULE module = LoadLibraryW(L"d3d10.dll");
    auto create = Core::GetProc<D3D10CreateDeviceAndSwapChainFn>(module, "D3D10CreateDeviceAndSwapChain");
    if (!create)
    {
        Logger::Error("[DX10] Install: d3d10.dll not loadable");
        return false;
    }

    HWND window = Core::CreateHiddenWindow();
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 2;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = window;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D10_DRIVER_TYPE drivers[] = {D3D10_DRIVER_TYPE_HARDWARE, D3D10_DRIVER_TYPE_WARP};
    IDXGISwapChain* swapChain = nullptr;
    ID3D10Device* device = nullptr;
    HRESULT hr = E_FAIL;
    for (D3D10_DRIVER_TYPE driver : drivers)
    {
        hr = create(nullptr, driver, nullptr, 0, D3D10_SDK_VERSION, &desc, &swapChain, &device, nullptr, nullptr);
        if (SUCCEEDED(hr))
            break;
    }

    bool hooked = false;
    if (SUCCEEDED(hr))
    {
        hooked |= InstallHook("IDXGISwapChain::Present", Core::VirtualMethod(swapChain, Dxgi::kPresentIndex),
                              reinterpret_cast<void*>(&HookPresent), reinterpret_cast<void**>(&oPresent));
        hooked |= InstallHook("IDXGISwapChain::ResizeBuffers",
                              Core::VirtualMethod(swapChain, Dxgi::kResizeBuffersIndex),
                              reinterpret_cast<void*>(&HookResizeBuffers),
                              reinterpret_cast<void**>(&oResizeBuffers));

        IDXGISwapChain1* swapChain1 = nullptr;
        if (SUCCEEDED(swapChain->QueryInterface(IID_PPV_ARGS(&swapChain1))))
        {
            hooked |= InstallHook("IDXGISwapChain1::Present1",
                                  Core::VirtualMethod(swapChain1, Dxgi::kPresent1Index),
                                  reinterpret_cast<void*>(&HookPresent1),
                                  reinterpret_cast<void**>(&oPresent1));
            swapChain1->Release();
        }
        IDXGISwapChain3* swapChain3 = nullptr;
        if (SUCCEEDED(swapChain->QueryInterface(IID_PPV_ARGS(&swapChain3))))
        {
            hooked |= InstallHook("IDXGISwapChain3::ResizeBuffers1",
                                  Core::VirtualMethod(swapChain3, Dxgi::kResizeBuffers1Index),
                                  reinterpret_cast<void*>(&HookResizeBuffers1),
                                  reinterpret_cast<void**>(&oResizeBuffers1));
            swapChain3->Release();
        }
        Core::Release(swapChain);
        Core::Release(device);
    }
    else
    {
        Logger::Error("[DX10] Install: dummy swap chain creation failed (0x{:08X})", static_cast<unsigned long>(hr));
    }

    if (window)
        DestroyWindow(window);
    if (module)
        FreeLibrary(module);

    if (!hooked)
    {
        Logger::Critical("[DX10] Install: no hook installed");
        return false;
    }

    Logger::Debug("[DX10] Install: {} hook(s) active", g_installedHooks.size());
    return true;
}

void DX10_Backend::Shutdown()
{
    static std::atomic<bool> g_shutdown{false};
    if (g_shutdown.exchange(true))
        return;
    Logger::Debug("[DX10] Shutdown: stopping");

    if (!g_installedHooks.empty())
    {
        // The GPU resources go first, on the rendering thread, at its next
        // present; then the hooks, once none of them is running.
        Core::TeardownOnRenderThread(&Teardown, 1000);
        for (const Hook& hook : g_installedHooks)
            HookHelper::DisableHook(hook.name, hook.target);
        Core::WaitForIdleHooks(2000);
        for (const Hook& hook : g_installedHooks)
            HookHelper::UninstallHook(hook.name, hook.target);
        g_installedHooks.clear();
    }

    // The rendering thread did not answer (the game stopped presenting, say):
    // release inline now that no hook can run.
    if (Core::TeardownPending() || g_device)
        Core::Guarded(&Teardown);
    Core::ResetTeardown();

    Logger::Debug("[DX10] Shutdown: done");
}
