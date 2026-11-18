//
// Created by superhuap on 2026/11/15.
//

#ifndef HACKFRAMEWORK_DX12_BACKEND_H
#define HACKFRAMEWORK_DX12_BACKEND_H

#include "backend/IRenderBackend.h"

class DX12_Backend : public IRenderBackend
{
public:
    bool Initialize() override;
    void Shutdown() override;
};

#endif // HACKFRAMEWORK_DX12_BACKEND_H
