//
// Created by superhuap on 2026/11/15.
//

#ifndef HACKFRAMEWORK_BACKENDFACTORY_H
#define HACKFRAMEWORK_BACKENDFACTORY_H

class IRenderBackend;

const char* GetBackendName();
IRenderBackend* CreateBackend();

#endif //HACKFRAMEWORK_BACKENDFACTORY_H
