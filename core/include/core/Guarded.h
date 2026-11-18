//
// Created by superhuap on 2026/11/18.
// Adapted from Universal-Dear-ImGui-Hook (MIT), src/uih.cpp and src/windows/platform.cpp.
//

#ifndef HACKFRAMEWORK_GUARDED_H
#define HACKFRAMEWORK_GUARDED_H

#include <type_traits>

#include "utils/InputLock.h"

namespace Core
{

    // Runs `work` on the calling thread; false when it raised an exception,
    // which was logged. Implemented once, in Guarded.cpp.
    bool RunGuarded(void (*work)(void*), void* context);

    // True once the overlay raised an exception, until the next Initialize().
    bool Faulted();
    void ResetFault();

    // Runs work of the overlay on a thread of the application. With MSVC, an
    // exception in it, from a renderer backend or the menu code, removes the
    // overlay instead of crashing the application. MinGW-w64 has no structured
    // exception handling (same limitation as Universal-Dear-ImGui-Hook): there
    // Guarded() only tracks the input locks, which an exception would skip.
    template <typename Work>
    bool Guarded(Work&& work)
    {
        const int depth = InputLock::Depth();
        const bool ok = RunGuarded([](void* context) { (*static_cast<std::remove_reference_t<Work>*>(context))(); }, &work);
        // The exception may have skipped the destructors of the locks `work` took.
        if (!ok)
            InputLock::ReleaseTo(depth);
        return ok;
    }

} // namespace Core

#endif // HACKFRAMEWORK_GUARDED_H
