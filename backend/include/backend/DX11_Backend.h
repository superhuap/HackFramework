//
// Created by superhuap on 2026/8/25.
//

#ifndef HACKFRAMEWORK_DX11_BACKEND_H
#define HACKFRAMEWORK_DX11_BACKEND_H

#include "IRenderBackend.h"

class DX11_Backend : public IRenderBackend
{
public:
    bool Initialize(HWND hWnd) override;
    void Shutdown() override;
};

#endif // HACKFRAMEWORK_DX11_BACKEND_H
