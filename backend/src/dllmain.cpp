//
// Created by superhuap on 2026/11/15.
//

#include <windows.h>

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved){
    if (fdwReason == DLL_PROCESS_ATTACH){
        DisableThreadLibraryCalls(hinstDLL);
        // HANDLE hThread = CreateThread(nullptr, 0, OnProcessAttach, hinstDLL, 0, nullptr);
        // if (hThread)
        //     CloseHandle(hThread);
    }
    else if (fdwReason == DLL_PROCESS_DETACH && !lpReserved){
        // OnProcessDetach(nullptr);
    }

    return TRUE;
}
