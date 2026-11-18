//
// Created by superhuap on 2026/11/18.
//

#include "backend/Lifecycle.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <thread>

#include <minhook.h>

#include "backend/BackendFactory.h"
#include "backend/IRenderBackend.h"
#include "utils/DllHelper.h"
#include "utils/InputHook.h"
#include "utils/Logger.h"

namespace
{

    std::unique_ptr<IRenderBackend> g_backend;
    std::mutex g_lifecycleMutex;

} // namespace

namespace Lifecycle
{

    // Runs on the thread that asked for the unload (never DllMain): stops the
    // overlay and the hooks, then lets the caller free the DLL.
    void PrepareUnload()
    {
        std::lock_guard<std::mutex> lock(g_lifecycleMutex);
        if (!g_backend)
            return;

        Logger::Debug("[HF] Unload: stopping the overlay");
        InputHook::shutting_down.store(true);
        InputHook::Remove();
        g_backend->Shutdown();
        g_backend.reset();
        MH_Uninitialize();
        Logger::Shutdown();
    }

    DWORD WINAPI OnProcessAttach(LPVOID)
    {
        Logger::Init();
        Logger::Debug("[HF] Attach: backend {}", GetBackendName());

        if (MH_Initialize() != MH_OK)
        {
            Logger::Critical("[HF] Attach: MinHook initialization failed");
            return 0;
        }

        InputHook::SetHandlers(nullptr, &PrepareUnload);

        g_backend = CreateBackend();
        if (!g_backend || !g_backend->Initialize())
        {
            Logger::Critical("[HF] Attach: backend initialization failed, unloading");
            g_backend.reset();
            MH_Uninitialize();
            Logger::Shutdown();
            // Give the async logger a moment before the DLL goes away.
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            DllHelper::UnloadDLL();
            return 0;
        }

        Logger::Debug("[HF] Attach: waiting for the first present");
        return 0;
    }

} // namespace Lifecycle
