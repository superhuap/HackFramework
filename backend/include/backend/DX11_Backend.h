//
// Created by superhuap on 2026/11/15.
//

#ifndef HACKFRAMEWORK_DX11_BACKEND_H
#define HACKFRAMEWORK_DX11_BACKEND_H

#include "backend/IRenderBackend.h"

class DX11_Backend : public IRenderBackend{
public:
    bool Initialize(HWND hWnd) override;
    void Shutdown() override;
};

#endif //HACKFRAMEWORK_DX11_BACKEND_H
