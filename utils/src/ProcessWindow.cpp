//
// Created by superhuap on 2026/8/25.
//

#include "utils/ProcessWindow.h"

#include <thread>

#include "utils/Logger.h"

namespace
{

    BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM lParam)
    {
        const auto isMainWindow = [handle]() {
            return GetWindow(handle, GW_OWNER) == nullptr && IsWindowVisible(handle);
        };

        DWORD processId = 0;
        GetWindowThreadProcessId(handle, &processId);

        if (GetCurrentProcessId() != processId || !isMainWindow() || handle == GetConsoleWindow())
            return TRUE;

        *reinterpret_cast<HWND*>(lParam) = handle;
        return FALSE;
    }

} // namespace

namespace Utils
{

    HWND TryGetProcessWindow()
    {
        HWND hwnd = nullptr;
        EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&hwnd));
        return hwnd;
    }

    HWND FindProcessWindow(bool blockUntilFound)
    {
        using namespace std::chrono_literals;

        HWND hwnd = TryGetProcessWindow();
        bool warned = false;

        while (!hwnd && blockUntilFound)
        {
            if (!warned)
            {
                LOG_WARN("Waiting for window to appear.");
                warned = true;
            }
            std::this_thread::sleep_for(200ms);
            hwnd = TryGetProcessWindow();
        }

        if (hwnd)
        {
            char name[128];
            GetWindowTextA(hwnd, name, RTL_NUMBER_OF(name));
            LOG_INFO(R"(Got window with name: '{}')", name);
        }

        return hwnd;
    }

} // namespace Utils
