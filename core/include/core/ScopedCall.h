//
// Created by superhuap on 2026/11/18.
// Adapted from Universal-Dear-ImGui-Hook (MIT), src/uih.cpp.
//

#ifndef HACKFRAMEWORK_SCOPEDCALL_H
#define HACKFRAMEWORK_SCOPEDCALL_H

namespace Core
{

    // Every hook keeps one of these alive while it runs so the shutdown
    // sequence can wait for in-flight calls before the hooks are removed.
    // Construct it as the first local of every hook callback.
    class ScopedCall
    {
    public:
        ScopedCall();
        ~ScopedCall();

        ScopedCall(const ScopedCall&) = delete;
        ScopedCall& operator=(const ScopedCall&) = delete;
    };

    // True when no hook callback is running, waiting up to timeoutMs.
    bool WaitForIdleHooks(unsigned timeoutMs);

} // namespace Core

#endif // HACKFRAMEWORK_SCOPEDCALL_H
