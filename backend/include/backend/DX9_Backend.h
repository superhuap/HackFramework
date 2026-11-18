//
// Created by superhuap on 2026/11/15.
//

#ifndef HACKFRAMEWORK_DX9_BACKEND_H
#define HACKFRAMEWORK_DX9_BACKEND_H

#include "backend/IRenderBackend.h"

class DX9_Backend : public IRenderBackend
{
public:
    bool Initialize() override;
    void Shutdown() override;
};

#endif // HACKFRAMEWORK_DX9_BACKEND_H
