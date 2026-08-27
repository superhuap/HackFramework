//
// Created by superhuap on 2026/8/25.
//

#ifndef HACKFRAMEWORK_BACKENDFACTORY_H
#define HACKFRAMEWORK_BACKENDFACTORY_H

class IRenderBackend;

const char* GetBackendName();
IRenderBackend* CreateBackend();

#endif // HACKFRAMEWORK_BACKENDFACTORY_H
