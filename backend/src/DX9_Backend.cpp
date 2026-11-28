//
// Created by superhuap on 2026/11/18.
// Adapted from Universal-Dear-ImGui-Hook (MIT), src/windows/d3d9.cpp.
//

#include "backend/DX9_Backend.h"

#include <d3d9.h>

#include <atomic>
#include <mutex>
#include <vector>

#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>

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

    using PresentFn = HRESULT(__stdcall*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
    using PresentExFn = HRESULT(__stdcall*)(IDirect3DDevice9Ex*, const RECT*, const RECT*, HWND, const RGNDATA*, DWORD);
    using SwapChainPresentFn = HRESULT(__stdcall*)(IDirect3DSwapChain9*, const RECT*, const RECT*, HWND, const RGNDATA*, DWORD);
    using ResetFn = HRESULT(__stdcall*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
    using ResetExFn = HRESULT(__stdcall*)(IDirect3DDevice9Ex*, D3DPRESENT_PARAMETERS*, D3DDISPLAYMODEEX*);
    using Direct3DCreate9ExFn = HRESULT(__stdcall*)(UINT, IDirect3D9Ex**);

    constexpr size_t kResetIndex = 16;
    constexpr size_t kPresentIndex = 17;
    constexpr size_t kPresentExIndex = 121;
    constexpr size_t kResetExIndex = 132;
    constexpr size_t kSwapChainPresentIndex = 3;

    // Devices created by Direct3DCreate9 and Direct3DCreate9Ex may not share
    // their methods, so each kind gets its own hooks.
    enum DeviceKind
    {
        kPlain,
        kEx,
        kKinds
    };

    HMODULE g_module = nullptr;
    PresentFn g_present[kKinds] = {};
    ResetFn g_reset[kKinds] = {};
    SwapChainPresentFn g_swapChainPresent[kKinds] = {};
    PresentExFn g_presentEx = nullptr;
    ResetExFn g_resetEx = nullptr;

    IDirect3DDevice9* g_device = nullptr;
    HWND g_boundWindow = nullptr;
    // Device::Present may go through SwapChain::Present: only the outer call renders.
    thread_local int t_presentDepth = 0;

    // One overlay thread at a time.
    std::mutex g_frameMutex;

    struct HookEntry
    {
        const char* name;
        void* target;
    };
    std::vector<HookEntry> g_installedHooks;

    void Teardown();

    HWND DeviceWindow(IDirect3DDevice9* device)
    {
        HWND window = nullptr;
        IDirect3DSwapChain9* swapChain = nullptr;
        if (SUCCEEDED(device->GetSwapChain(0, &swapChain)))
        {
            D3DPRESENT_PARAMETERS present{};
            if (SUCCEEDED(swapChain->GetPresentParameters(&present)))
                window = present.hDeviceWindow;
            swapChain->Release();
        }
        if (window)
            return window;
        D3DDEVICE_CREATION_PARAMETERS params{};
        if (SUCCEEDED(device->GetCreationParameters(&params)))
            window = params.hFocusWindow;
        return window;
    }

    // Releases the renderer and the ImGui context. Runs on the rendering
    // thread, from ServiceTeardown(); idempotent.
    void Teardown()
    {
        if (!g_device)
            return;
        Logger::Debug("[DX9] Teardown: releasing the overlay");
        if (ImGui::GetCurrentContext() && ImGui::GetIO().BackendRendererUserData)
            ImGui_ImplDX9_Shutdown();
        Menu::Shutdown();
        Core::Release(g_device);
        g_boundWindow = nullptr;
    }

    bool Init(IDirect3DDevice9* device)
    {
        // A new device: release the previous overlay first.
        Teardown();

        HWND window = DeviceWindow(device);
        if (!window || !Menu::Initialize(window))
            return false;

        ImGui_ImplDX9_Init(device);
        device->AddRef();
        g_device = device;
        g_boundWindow = window;
        Logger::Debug("[DX9] Init: bound to device {:p}, window {:p}", reinterpret_cast<void*>(device),
                      reinterpret_cast<void*>(window));
        return true;
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

    // Draws on the back buffer of the implicit swap chain, outside of the
    // application's scenes, whatever render target is bound.
    void Render(IDirect3DDevice9* device)
    {
        if (device != g_device && !Init(device))
            return;

        // Before any reference to the back buffer, which an exception in the frame would leak.
        ImGui_ImplDX9_NewFrame();
        ImDrawData* drawData = Frame();

        IDirect3DSurface9* backBuffer = nullptr;
        if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)))
            return;
        IDirect3DSurface9* previous = nullptr;
        device->GetRenderTarget(0, &previous);
        D3DVIEWPORT9 viewport{};
        device->GetViewport(&viewport);
        device->SetRenderTarget(0, backBuffer);
        // sRGB writes would encode the overlay's colors, already sRGB, a second time.
        DWORD srgbWrite = FALSE;
        device->GetRenderState(D3DRS_SRGBWRITEENABLE, &srgbWrite);
        device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
        if (SUCCEEDED(device->BeginScene()))
        {
            // A fault in the backend must not leave the back buffer bound or referenced.
            Core::Guarded([&] { ImGui_ImplDX9_RenderDrawData(drawData); });
            device->EndScene();
        }
        device->SetRenderState(D3DRS_SRGBWRITEENABLE, srgbWrite);
        device->SetRenderTarget(0, previous);
        device->SetViewport(&viewport);
        Core::Release(previous);
        backBuffer->Release();
    }

    bool MayRender()
    {
        return !InputHook::shutting_down.load() && !Core::Faulted() &&
               (!g_boundWindow || IsWindow(g_boundWindow));
    }

    void OnPresent(IDirect3DDevice9* device)
    {
        if (t_presentDepth++ == 0 && MayRender())
        {
            if (g_frameMutex.try_lock())
            {
                Core::Guarded([&] { Render(device); });
                Core::ServiceTeardown(&Teardown);
                g_frameMutex.unlock();
            }
        }
    }

    void AfterPresent()
    {
        if (--t_presentDepth == 0)
            Core::ServiceTeardown(&Teardown);
    }

    template <DeviceKind Kind>
    HRESULT STDMETHODCALLTYPE HookPresent(IDirect3DDevice9* device, const RECT* source, const RECT* dest, HWND window,
                                          const RGNDATA* dirty)
    {
        Core::ScopedCall call;
        if (!InputHook::shutting_down.load())
            Core::Guarded([&] { OnPresent(device); });
        HRESULT hr = g_present[Kind](device, source, dest, window, dirty);
        Core::Guarded([&] { AfterPresent(); });
        return hr;
    }

    HRESULT STDMETHODCALLTYPE HookPresentEx(IDirect3DDevice9Ex* device, const RECT* source, const RECT* dest,
                                            HWND window, const RGNDATA* dirty, DWORD flags)
    {
        Core::ScopedCall call;
        if (!InputHook::shutting_down.load())
            Core::Guarded([&] { OnPresent(device); });
        HRESULT hr = g_presentEx(device, source, dest, window, dirty, flags);
        Core::Guarded([&] { AfterPresent(); });
        return hr;
    }

    template <DeviceKind Kind>
    HRESULT STDMETHODCALLTYPE HookSwapChainPresent(IDirect3DSwapChain9* swapChain, const RECT* source, const RECT* dest,
                                                   HWND window, const RGNDATA* dirty, DWORD flags)
    {
        Core::ScopedCall call;
        // Additional swap chains present other windows: only the implicit one gets the overlay.
        IDirect3DDevice9* device = nullptr;
        IDirect3DSwapChain9* implicit = nullptr;
        if (SUCCEEDED(swapChain->GetDevice(&device)))
        {
            device->GetSwapChain(0, &implicit);
            if (implicit == swapChain)
            {
                if (!InputHook::shutting_down.load())
                    Core::Guarded([&] { OnPresent(device); });
            }
            else
            {
                ++t_presentDepth;
            }
            Core::Release(implicit);
        }
        else
        {
            ++t_presentDepth;
        }
        HRESULT hr = g_swapChainPresent[Kind](swapChain, source, dest, window, dirty, flags);
        Core::Guarded([&] { AfterPresent(); });
        Core::Release(device);
        return hr;
    }

    void BeforeReset(IDirect3DDevice9* device)
    {
        if (device == g_device)
            ImGui_ImplDX9_InvalidateDeviceObjects();
    }

    void AfterReset(IDirect3DDevice9* device, HRESULT hr)
    {
        if (device == g_device && SUCCEEDED(hr))
            ImGui_ImplDX9_CreateDeviceObjects();
    }

    template <DeviceKind Kind>
    HRESULT STDMETHODCALLTYPE HookReset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* params)
    {
        Core::ScopedCall call;
        if (!InputHook::shutting_down.load())
            Core::Guarded([&] { BeforeReset(device); });
        HRESULT hr = g_reset[Kind](device, params);
        if (!InputHook::shutting_down.load())
            Core::Guarded([&] { AfterReset(device, hr); });
        return hr;
    }

    HRESULT STDMETHODCALLTYPE HookResetEx(IDirect3DDevice9Ex* device, D3DPRESENT_PARAMETERS* params, D3DDISPLAYMODEEX* mode)
    {
        Core::ScopedCall call;
        if (!InputHook::shutting_down.load())
            Core::Guarded([&] { BeforeReset(device); });
        HRESULT hr = g_resetEx(device, params, mode);
        if (!InputHook::shutting_down.load())
            Core::Guarded([&] { AfterReset(device, hr); });
        return hr;
    }

    struct DeviceMethods
    {
        void* present = nullptr;
        void* reset = nullptr;
        void* swapChainPresent = nullptr;
    };

    DeviceMethods Methods(IDirect3DDevice9* device)
    {
        DeviceMethods methods;
        methods.present = Core::VirtualMethod(device, kPresentIndex);
        methods.reset = Core::VirtualMethod(device, kResetIndex);
        IDirect3DSwapChain9* swapChain = nullptr;
        if (SUCCEEDED(device->GetSwapChain(0, &swapChain)))
        {
            methods.swapChainPresent = Core::VirtualMethod(swapChain, kSwapChainPresentIndex);
            swapChain->Release();
        }
        return methods;
    }

    bool InstallHook(const char* name, void* target, void* detour, void** original)
    {
        if (!HookHelper::InstallHook(name, target, detour, original))
            return false;
        g_installedHooks.push_back({name, target});
        return true;
    }

    // Methods already hooked through the other kind of device are left alone.
    template <DeviceKind Kind>
    bool HookDevice(const DeviceMethods& methods, const DeviceMethods& hooked)
    {
        bool present = methods.present == hooked.present ||
                       InstallHook("IDirect3DDevice9::Present", methods.present,
                                   reinterpret_cast<void*>(&HookPresent<Kind>), reinterpret_cast<void**>(&g_present[Kind]));
        if (methods.reset != hooked.reset)
            InstallHook("IDirect3DDevice9::Reset", methods.reset, reinterpret_cast<void*>(&HookReset<Kind>),
                        reinterpret_cast<void**>(&g_reset[Kind]));
        if (methods.swapChainPresent && methods.swapChainPresent != hooked.swapChainPresent)
            InstallHook("IDirect3DSwapChain9::Present", methods.swapChainPresent,
                        reinterpret_cast<void*>(&HookSwapChainPresent<Kind>),
                        reinterpret_cast<void**>(&g_swapChainPresent[Kind]));
        return present;
    }

    D3DPRESENT_PARAMETERS DummyParameters(HWND window)
    {
        D3DPRESENT_PARAMETERS present{};
        present.Windowed = TRUE;
        present.SwapEffect = D3DSWAPEFFECT_DISCARD;
        present.hDeviceWindow = window;
        return present;
    }

    constexpr DWORD kDummyFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_NOWINDOWCHANGES;

} // namespace

bool DX9_Backend::Initialize()
{
    Logger::Debug("[DX9] Install: starting");

    Core::ResetFault();
    InputHook::shutting_down.store(false);

    g_module = LoadLibraryW(L"d3d9.dll");
    auto create = Core::GetProc<IDirect3D9*(WINAPI*)(UINT)>(g_module, "Direct3DCreate9");
    auto createEx = Core::GetProc<Direct3DCreate9ExFn>(g_module, "Direct3DCreate9Ex");
    if (!create)
    {
        Logger::Error("[DX9] Install: d3d9.dll not loadable");
        return false;
    }

    HWND window = Core::CreateHiddenWindow();
    DeviceMethods plain;
    bool hooked = false;

    if (IDirect3D9* d3d = create(D3D_SDK_VERSION))
    {
        D3DPRESENT_PARAMETERS present = DummyParameters(window);
        IDirect3DDevice9* device = nullptr;
        HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, kDummyFlags, &present, &device);
        if (SUCCEEDED(hr))
        {
            DeviceMethods methods = Methods(device);
            if (HookDevice<kPlain>(methods, DeviceMethods{}))
            {
                plain = methods;
                hooked = true;
            }
            device->Release();
        }
        else
        {
            Logger::Error("[DX9] Install: dummy device creation failed (0x{:08X})", static_cast<unsigned long>(hr));
        }
        d3d->Release();
    }

    IDirect3D9Ex* d3dEx = nullptr;
    if (createEx && SUCCEEDED(createEx(D3D_SDK_VERSION, &d3dEx)))
    {
        D3DPRESENT_PARAMETERS present = DummyParameters(window);
        IDirect3DDevice9Ex* device = nullptr;
        HRESULT hr =
            d3dEx->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, kDummyFlags, &present, nullptr, &device);
        if (SUCCEEDED(hr))
        {
            hooked = HookDevice<kEx>(Methods(device), plain) || hooked;
            InstallHook("IDirect3DDevice9Ex::PresentEx", Core::VirtualMethod(device, kPresentExIndex),
                        reinterpret_cast<void*>(&HookPresentEx), reinterpret_cast<void**>(&g_presentEx));
            InstallHook("IDirect3DDevice9Ex::ResetEx", Core::VirtualMethod(device, kResetExIndex),
                        reinterpret_cast<void*>(&HookResetEx), reinterpret_cast<void**>(&g_resetEx));
            device->Release();
        }
        else
        {
            Logger::Error("[DX9] Install: dummy D3D9Ex device creation failed (0x{:08X})",
                          static_cast<unsigned long>(hr));
        }
        d3dEx->Release();
    }

    if (window)
        DestroyWindow(window);

    if (!hooked)
    {
        Logger::Critical("[DX9] Install: no hook installed");
        return false;
    }

    Logger::Debug("[DX9] Install: {} hook(s) active", g_installedHooks.size());
    return true;
}

void DX9_Backend::Shutdown()
{
    static std::atomic<bool> g_shutdown{false};
    if (g_shutdown.exchange(true))
        return;
    Logger::Debug("[DX9] Shutdown: stopping");

    if (!g_installedHooks.empty())
    {
        // The GPU resources go first, on the rendering thread, at its next
        // present; then the hooks, once none of them is running.
        Core::TeardownOnRenderThread(&Teardown, 1000);
        for (const HookEntry& hook : g_installedHooks)
            HookHelper::DisableHook(hook.name, hook.target);
        if (!Core::WaitForIdleHooks(2000))
        {
            // A detour is still running: removing its trampoline would free
            // code a thread is executing. Leave the hooks disabled; the unload
            // path keeps the library loaded until they drain.
            Logger::Error("[DX9] Shutdown: hooks still running, left disabled");
            return;
        }
        for (const HookEntry& hook : g_installedHooks)
            HookHelper::UninstallHook(hook.name, hook.target);
        g_installedHooks.clear();
    }

    // The rendering thread did not answer (the game stopped presenting, say):
    // release inline now that no hook can run.
    if (Core::TeardownPending() || g_device)
        Core::Guarded(&Teardown);
    Core::ResetTeardown();

    if (g_module)
    {
        FreeLibrary(g_module);
        g_module = nullptr;
    }

    Logger::Debug("[DX9] Shutdown: done");
}
