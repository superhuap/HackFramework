//
// Created by superhuap on 2026/8/25.
//

#ifndef HACKFRAMEWORK_IRENDERBACKEND_H
#define HACKFRAMEWORK_IRENDERBACKEND_H

#include <windows.h>
#include <imgui.h>

class IRenderBackend
{
public:
    virtual ~IRenderBackend() = default;

    virtual bool Initialize(HWND hWnd) = 0;
    virtual void Shutdown() = 0;
    virtual void RenderFrame() = 0;
};

#endif //HACKFRAMEWORK_IRENDERBACKEND_H
