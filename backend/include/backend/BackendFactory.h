//
// Created by superhuap on 2026/11/18.
//

#ifndef HACKFRAMEWORK_BACKENDFACTORY_H
#define HACKFRAMEWORK_BACKENDFACTORY_H

#include <memory>

class IRenderBackend;

// One backend per DLL: the BACKEND_* compile definition selects the
// implementation. Returns null when no backend was compiled in.
std::unique_ptr<IRenderBackend> CreateBackend();

const char* GetBackendName();

#endif // HACKFRAMEWORK_BACKENDFACTORY_H
