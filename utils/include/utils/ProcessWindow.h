//
// Created by superhuap on 2026/8/25.
//

#ifndef HACKFRAMEWORK_PROCESSWINDOW_H
#define HACKFRAMEWORK_PROCESSWINDOW_H

#include <windows.h>

namespace Utils
{

    HWND TryGetProcessWindow();
    HWND FindProcessWindow(bool blockUntilFound);

} // namespace Utils

#endif // HACKFRAMEWORK_PROCESSWINDOW_H
