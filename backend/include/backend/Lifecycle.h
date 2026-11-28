//
// Created by superhuap on 2026/11/18.
//

#ifndef HACKFRAMEWORK_LIFECYCLE_H
#define HACKFRAMEWORK_LIFECYCLE_H

#include <windows.h>

namespace Lifecycle
{

    // DLL_PROCESS_ATTACH worker: logging, MinHook, backend and hooks startup.
    DWORD WINAPI OnProcessAttach(LPVOID);

    // Full stop of the overlay and the hooks, for the unload path.
    // Returns false when a hook callback did not drain: the caller must keep
    // the library loaded, since freeing it would free code a thread still runs.
    bool PrepareUnload();

} // namespace Lifecycle

#endif // HACKFRAMEWORK_LIFECYCLE_H
