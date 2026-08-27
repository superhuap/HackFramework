//
// Created by superhuap on 2026/8/25.
//

#include "utils/DllHelper.h"

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

namespace
{

    void (*g_prepareFn)() = nullptr;

    DWORD WINAPI UnloadThread(LPVOID)
    {
        if (g_prepareFn)
            g_prepareFn();
        FreeLibraryAndExitThread(reinterpret_cast<HMODULE>(&__ImageBase), 0);
        return 0;
    }

} // namespace

namespace Utils
{

    HMODULE GetCurrentImageBase()
    {
        return reinterpret_cast<HMODULE>(&__ImageBase);
    }

    void UnloadDLL(void (*prepare)())
    {
        g_prepareFn = prepare;
        HANDLE hThread = CreateThread(nullptr, 0, UnloadThread, nullptr, 0, nullptr);
        if (hThread)
            CloseHandle(hThread);
    }

} // namespace Utils
