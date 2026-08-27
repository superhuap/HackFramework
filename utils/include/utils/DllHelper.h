//
// Created by superhuap on 2026/8/25.
//

#ifndef HACKFRAMEWORK_DLLHELPER_H
#define HACKFRAMEWORK_DLLHELPER_H

#include <windows.h>

namespace Utils
{

    HMODULE GetCurrentImageBase();
    void UnloadDLL(void (*prepare)() = nullptr);

} // namespace Utils

#endif // HACKFRAMEWORK_DLLHELPER_H
