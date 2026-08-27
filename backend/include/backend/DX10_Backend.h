//
// Created by superhuap on 2026/8/25.
//

#ifndef HACKFRAMEWORK_DX10_BACKEND_H
#define HACKFRAMEWORK_DX10_BACKEND_H

#include "IRenderBackend.h"

class DX10_Backend : public IRenderBackend
{
public:
    bool Initialize(HWND hWnd) override;
    void Shutdown() override;
};

#endif // HACKFRAMEWORK_DX10_BACKEND_H
