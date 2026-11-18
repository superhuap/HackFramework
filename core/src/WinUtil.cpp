//
// Created by superhuap on 2026/11/18.
// Adapted from Universal-Dear-ImGui-Hook (MIT), src/windows/dxgi.cpp.
//

#include "core/WinUtil.h"

namespace Core
{

    HWND CreateHiddenWindow()
    {
        return CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 64, 64, nullptr, nullptr, nullptr, nullptr);
    }

} // namespace Core
