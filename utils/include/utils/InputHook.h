//
// Created by superhuap on 2026/8/25.
//

#ifndef HACKFRAMEWORK_INPUTHOOK_H
#define HACKFRAMEWORK_INPUTHOOK_H

#include <windows.h>
#include <atomic>

namespace Utils::Input
{

    inline std::atomic<bool> menu_visible{true};
    inline std::atomic<bool> shutting_down{false};

    using RehookHandler = void (*)(HWND staleWindow);
    using UnloadHandler = void (*)();

    void Install(HWND hWnd, RehookHandler rehookHandler = nullptr, UnloadHandler unloadHandler = nullptr);
    void Remove();

} // namespace Utils::Input

#endif // HACKFRAMEWORK_INPUTHOOK_H
