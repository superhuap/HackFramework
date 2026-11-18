//
// Created by superhuap on 2026/11/17.
//

#ifndef HACKFRAMEWORK_IRENDERBACKEND_H
#define HACKFRAMEWORK_IRENDERBACKEND_H

struct HWND__; using HWND = HWND__*;

class IRenderBackend{
public:
    virtual ~IRenderBackend() = default;

    virtual bool Initialize(HWND hWnd) = 0;
    virtual void Shutdown() = 0;
};

#endif //HACKFRAMEWORK_IRENDERBACKEND_H
