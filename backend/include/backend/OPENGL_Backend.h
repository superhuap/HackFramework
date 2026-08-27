//
// Created by superhuap on 2026/8/25.
//

#ifndef HACKFRAMEWORK_OPENGL_BACKEND_H
#define HACKFRAMEWORK_OPENGL_BACKEND_H

#include "IRenderBackend.h"

class OPENGL_Backend : public IRenderBackend
{
public:
    OPENGL_Backend() = default;
    ~OPENGL_Backend() override = default;

    bool Initialize(HWND hWnd) override;
    void Shutdown() override;

private:
    static BOOL WINAPI Hook_wglSwapBuffers(HDC hdc);

    static OPENGL_Backend* s_instance;
};

#endif // HACKFRAMEWORK_OPENGL_BACKEND_H
