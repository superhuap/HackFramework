//
// Created by superhuap on 2026/11/18.
// Adapted from Universal-Dear-ImGui-Hook (MIT), src/windows/platform.cpp,
// src/windows/dxgi.cpp and src/windows/input.cpp.
//

#include "core/WinUtil.h"

namespace
{

    struct WindowSearch
    {
        DWORD process;
        long width;
        long height;
        HWND best;
        long bestArea;
        HWND sized;
    };

    BOOL CALLBACK EnumWindowsProc(HWND window, LPARAM param)
    {
        auto* search = reinterpret_cast<WindowSearch*>(param);
        DWORD process = 0;
        GetWindowThreadProcessId(window, &process);
        if (process != search->process || !IsWindowVisible(window) || GetWindow(window, GW_OWNER))
            return TRUE;
        RECT rect{};
        GetClientRect(window, &rect);
        if (search->width && rect.right == search->width && rect.bottom == search->height && !search->sized)
            search->sized = window;
        long area = (rect.right - rect.left) * (rect.bottom - rect.top);
        if (area > search->bestArea)
        {
            search->best = window;
            search->bestArea = area;
        }
        return TRUE;
    }

} // namespace

namespace Core
{

    HWND CreateHiddenWindow()
    {
        return CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 64, 64, nullptr, nullptr, nullptr, nullptr);
    }

    HWND FindProcessWindow(uint32_t width, uint32_t height)
    {
        WindowSearch search{GetCurrentProcessId(), static_cast<long>(width), static_cast<long>(height), nullptr, -1,
                            nullptr};
        HWND foreground = GetForegroundWindow();
        DWORD process = 0;
        bool ownForeground =
            foreground && GetWindowThreadProcessId(foreground, &process) && process == search.process;
        RECT client{};
        if (ownForeground &&
            (!width || (GetClientRect(foreground, &client) && client.right == search.width &&
                        client.bottom == search.height)))
            return foreground;
        EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&search));
        if (search.sized)
            return search.sized;
        return ownForeground ? foreground : search.best;
    }

} // namespace Core
