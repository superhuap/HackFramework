//
// Created by superhuap on 2026/11/18.
//

#ifndef HACKFRAMEWORK_IRENDERBACKEND_H
#define HACKFRAMEWORK_IRENDERBACKEND_H

class IRenderBackend
{
public:
    virtual ~IRenderBackend() = default;

    IRenderBackend(const IRenderBackend&) = delete;
    IRenderBackend& operator=(const IRenderBackend&) = delete;

    // Worker thread: installs the hooks. The backend binds to the game's real
    // swap chain at its first Present (probing the device behind it), so no
    // window handle is needed and no dummy window outlives the install.
    virtual bool Initialize() = 0;

    // Idempotent full stop: releases the GPU resources on the rendering
    // thread, then disables and removes the hooks once none is in flight.
    // Never call from DllMain or from inside a hook.
    virtual void Shutdown() = 0;

protected:
    IRenderBackend() = default;
};

#endif // HACKFRAMEWORK_IRENDERBACKEND_H
