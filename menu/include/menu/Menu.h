//
// Created by superhuap on 2026/11/18.
// Ported from the previous HackFramework iteration.
//

#ifndef HACKFRAMEWORK_MENU_H
#define HACKFRAMEWORK_MENU_H

#include <windows.h>

namespace Menu
{

    // Creates the ImGui context, binds the platform backend to `hWnd` and
    // starts the features. Callable again with a new window to rebind (e.g.
    // when the game recreates its swap chain on another window).
    bool Initialize(HWND hWnd);

    // Draws the menu window when it is visible and ticks the features every
    // frame, whether the menu is open or not. Called by the rendering backend
    // between ImGui::NewFrame() and ImGui::Render(), under InputLock.
    void Render();

    // Stops the features, detaches the platform backend and destroys the
    // ImGui context. Idempotent.
    void Shutdown();

} // namespace Menu

#endif // HACKFRAMEWORK_MENU_H
