//
// Created by superhuap on 2026/11/18.
// Adapted from Universal-Dear-ImGui-Hook (MIT), src/uih.cpp.
//

#include "core/Teardown.h"
#include "core/Guarded.h"

#include <atomic>
#include <condition_variable>
#include <chrono>
#include <mutex>

namespace Core
{

    namespace
    {
        std::atomic<bool> g_pending{false};
        std::mutex g_signalMutex;
        std::condition_variable g_signal;
        bool g_done = false;
    }

    void ServiceTeardown(void (*teardown)())
    {
        if (!g_pending.exchange(false))
            return;
        Guarded(teardown);
        {
            std::lock_guard<std::mutex> lock(g_signalMutex);
            g_done = true;
        }
        g_signal.notify_all();
    }

    bool TeardownOnRenderThread(void (*teardown)(), unsigned timeoutMs)
    {
        {
            std::lock_guard<std::mutex> lock(g_signalMutex);
            g_done = false;
        }
        g_pending.store(true, std::memory_order_release);

        std::unique_lock<std::mutex> lock(g_signalMutex);
        if (g_signal.wait_for(lock, std::chrono::milliseconds(timeoutMs), [] { return g_done; }))
            return true;
        // The rendering thread did not answer in time: the caller decides
        // whether to run the teardown itself once the hooks are gone.
        return !g_pending.exchange(false);
    }

    bool TeardownPending() { return g_pending.load(std::memory_order_acquire); }

    void ResetTeardown() { g_pending.store(false, std::memory_order_release); }

} // namespace Core
