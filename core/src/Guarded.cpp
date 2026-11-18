//
// Created by superhuap on 2026/11/18.
// Adapted from Universal-Dear-ImGui-Hook (MIT), src/uih.cpp and src/windows/platform.cpp.
//

#include "core/Guarded.h"

#include "utils/Logger.h"

#include <windows.h>
#include <atomic>

namespace Core
{

    namespace
    {
        std::atomic<bool> g_faulted{false};
    }

    bool RunGuarded(void (*work)(void*), void* context)
    {
#if defined(_MSC_VER)
        DWORD code = 0;
        __try
        {
            work(context);
            return true;
        }
        __except (code = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)
        {
        }
        if (code == EXCEPTION_STACK_OVERFLOW)
            _resetstkoflw();
        g_faulted.store(true, std::memory_order_relaxed);
        Logger::Critical("[Core] Fault: exception 0x{:08X} in the overlay", code);
        // Rendering stays suppressed until the backend re-initializes itself
        // (ResetFault) or the DLL is unloaded. A full auto-removal protocol
        // (StopUntilMenuInput in uih) can be added on top of Faulted().
        return false;
#else
        // MinGW-w64 has no structured exception handling (Universal-Dear-ImGui-Hook
        // ships the same fallback). See the plan decision log for the MSVC option.
        work(context);
        return true;
#endif
    }

    bool Faulted() { return g_faulted.load(std::memory_order_relaxed); }

    void ResetFault() { g_faulted.store(false, std::memory_order_relaxed); }

} // namespace Core
