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
    void PrepareUnload();

} // namespace Lifecycle

#endif // HACKFRAMEWORK_LIFECYCLE_H
