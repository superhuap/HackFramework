//
// Created by superhuap on 2026/11/15.
//

#ifndef HACKFRAMEWORK_OPENGL_BACKEND_H
#define HACKFRAMEWORK_OPENGL_BACKEND_H

#include "backend/IRenderBackend.h"

class OPENGL_Backend : public IRenderBackend{
public:
    bool Initialize(HWND hWnd) override;
    void Shutdown() override;
};

#endif //HACKFRAMEWORK_OPENGL_BACKEND_H
