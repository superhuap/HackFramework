//
// Created by superhuap on 2026/11/18.
// Adapted from Universal-Dear-ImGui-Hook (MIT), src/uih.cpp
// (ServiceTeardown / TeardownOnRenderThread, simplified for the
// one-backend-per-DLL model).
//

#ifndef HACKFRAMEWORK_TEARDOWN_H
#define HACKFRAMEWORK_TEARDOWN_H

namespace Core
{

    // Called by the backend inside the present hook: runs `teardown` when a
    // teardown has been requested, once. Must run on the rendering thread.
    void ServiceTeardown(void (*teardown)());

    // Requests the rendering thread to run `teardown` at its next present and
    // waits until it did. False on timeout, or when the hooks are already
    // removed and no present can come (check TeardownPending() afterwards).
    // Not from DllMain and not from inside a hook.
    bool TeardownOnRenderThread(void (*teardown)(), unsigned timeoutMs = 1000);

    // True while a teardown was requested but not served yet.
    bool TeardownPending();

    // Clears a pending request (e.g. after the caller ran the teardown itself).
    void ResetTeardown();

} // namespace Core

#endif // HACKFRAMEWORK_TEARDOWN_H
