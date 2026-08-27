//
// Created by superhuap on 2026/8/25.
//

#ifndef HACKFRAMEWORK_MENU_H
#define HACKFRAMEWORK_MENU_H

#include <windows.h>

namespace Menu
{

    bool Initialize(HWND hWnd);
    void Render();
    void Shutdown();

} // namespace Menu

#endif // HACKFRAMEWORK_MENU_H