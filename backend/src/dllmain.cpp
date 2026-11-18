//
// Created by superhuap on 2026/11/18.
//

#include <Windows.h>

#include "backend/Lifecycle.h"

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hinstDLL);
        HANDLE hThread = CreateThread(nullptr, 0, Lifecycle::OnProcessAttach, hinstDLL, 0, nullptr);
        if (hThread)
            CloseHandle(hThread);
    }
    // DLL_PROCESS_DETACH: the unload path ran on its own thread before
    // FreeLibraryAndExitThread. A static detach (process exiting, or a
    // FreeLibrary by the host) must not tear the overlay down here: DllMain
    // holds the loader lock, and the rendering thread may be inside a hook.
    return TRUE;
}
