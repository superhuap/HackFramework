//
// Created by superhuap on 2026/8/25.
//

#include <Windows.h>

#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

#include <minhook.h>

#include "backend/BackendFactory.h"
#include "backend/IRenderBackend.h"
#include "utils/DllHelper.h"
#include "utils/InputHook.h"
#include "utils/Logger.h"
#include "utils/ProcessWindow.h"

namespace
{

    IRenderBackend* g_backend = nullptr;
    HWND g_window = nullptr;
    std::mutex g_rehookMutex;

    void TriggerRehook(HWND staleWindow);
    DWORD WINAPI ReinitializeGraphicalHooks(LPVOID);
    void PrepareUnload();

    bool StartHooks(HWND preferredWindow = nullptr)
    {
        g_backend = CreateBackend();
        if (!g_backend)
            return false;

        g_window = preferredWindow ? preferredWindow : Utils::FindProcessWindow(true);
        if (!g_window)
        {
            LOG_ERROR("Failed to find process window");
            delete g_backend;
            g_backend = nullptr;
            return false;
        }

        if (!g_backend->Initialize(g_window))
        {
            LOG_ERROR("Failed to initialize {} backend", GetBackendName());
            delete g_backend;
            g_backend = nullptr;
            return false;
        }

        Utils::Input::Install(g_window, &TriggerRehook, &PrepareUnload);
        LOG_INFO("Hooks initialized");
        return true;
    }

    void StopHooks()
    {
        Utils::Input::shutting_down.store(true);
        Utils::Input::Remove();

        MH_DisableHook(MH_ALL_HOOKS);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (g_backend)
        {
            g_backend->Shutdown();
            delete g_backend;
            g_backend = nullptr;
        }
        g_window = nullptr;
    }

    void TriggerRehook(HWND staleWindow)
    {
        HANDLE hThread = CreateThread(nullptr, 0, ReinitializeGraphicalHooks,
                                      reinterpret_cast<LPVOID>(staleWindow), 0, nullptr);
        if (hThread)
            CloseHandle(hThread);
    }

    DWORD WINAPI ReinitializeGraphicalHooks(LPVOID lpParam)
    {
        std::lock_guard<std::mutex> lock(g_rehookMutex);

        LOG_WARN("Hooks will reinitialize!");

        const HWND staleWindow = reinterpret_cast<HWND>(lpParam);
        HWND newWindow = Utils::TryGetProcessWindow();
        while (!newWindow || newWindow == staleWindow)
        {
            newWindow = Utils::TryGetProcessWindow();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        StopHooks();
        StartHooks(newWindow);

        Utils::Input::shutting_down.store(false);
        Utils::Input::menu_visible.store(true);
        return 0;
    }

    void PrepareUnload()
    {
        StopHooks();
        MH_Uninitialize();
        Logger::Shutdown();
    }

    DWORD WINAPI OnProcessAttach(LPVOID)
    {
        Logger::Init();

        LOG_INFO("Rendering backend: {}", GetBackendName());

        if (MH_Initialize() != MH_OK)
        {
            LOG_ERROR("Failed to initialize MinHook");
            return 0;
        }

        if (!StartHooks())
        {
            LOG_WARN("Looks like you forgot to set a backend. Will unload after pressing enter...");
            std::cin.get();
            Utils::UnloadDLL(&PrepareUnload);
        }

        return 0;
    }

    DWORD WINAPI OnProcessDetach(LPVOID)
    {
        PrepareUnload();
        return 0;
    }

} // namespace

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hinstDLL);
        HANDLE hThread = CreateThread(nullptr, 0, OnProcessAttach, hinstDLL, 0, nullptr);
        if (hThread)
            CloseHandle(hThread);
    }
    else if (fdwReason == DLL_PROCESS_DETACH && !lpReserved)
    {
        OnProcessDetach(nullptr);
    }

    return TRUE;
}
