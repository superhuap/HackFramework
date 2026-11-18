//
// Created by superhuap on 2026/11/18.
// Ported from the previous HackFramework iteration.
//

#ifndef HACKFRAMEWORK_DLLHELPER_H
#define HACKFRAMEWORK_DLLHELPER_H

#include <windows.h>

namespace DllHelper
{

    HMODULE GetCurrentImageBase();
    void UnloadDLL(void (*prepare)() = nullptr);

} // namespace DllHelper

#endif // HACKFRAMEWORK_DLLHELPER_H
