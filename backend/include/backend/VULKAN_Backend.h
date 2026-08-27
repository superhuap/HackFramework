//
// Created by superhuap on 2026/8/25.
//

#ifndef HACKFRAMEWORK_VULKAN_BACKEND_H
#define HACKFRAMEWORK_VULKAN_BACKEND_H

#include "IRenderBackend.h"

class VULKAN_Backend : public IRenderBackend
{
public:
    bool Initialize(HWND hWnd) override;
    void Shutdown() override;
};

#endif // HACKFRAMEWORK_VULKAN_BACKEND_H
