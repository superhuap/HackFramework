//
// Created by superhuap on 2026/11/18.
// Adapted from Universal-Dear-ImGui-Hook (MIT), src/uih.cpp.
//

#include "core/ScopedCall.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace Core
{

    namespace
    {
        std::atomic<int> g_inFlight{0};
    }

    ScopedCall::ScopedCall() { ++g_inFlight; }
    ScopedCall::~ScopedCall() { --g_inFlight; }

    bool WaitForIdleHooks(unsigned timeoutMs)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (g_inFlight > 0)
        {
            if (std::chrono::steady_clock::now() >= deadline)
                return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return true;
    }

} // namespace Core
