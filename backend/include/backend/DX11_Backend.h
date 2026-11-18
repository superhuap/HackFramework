//
// Created by superhuap on 2026/11/18.
//

#ifndef HACKFRAMEWORK_DX11_BACKEND_H
#define HACKFRAMEWORK_DX11_BACKEND_H

#include "backend/IRenderBackend.h"

class DX11_Backend : public IRenderBackend
{
public:
    bool Initialize() override;
    void Shutdown() override;
};

#endif // HACKFRAMEWORK_DX11_BACKEND_H
