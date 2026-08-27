//
// Created by superhuap on 2026/8/25.
//

#ifndef HACKFRAMEWORK_MINHOOKHELPER_H
#define HACKFRAMEWORK_MINHOOKHELPER_H

#include <minhook.h>

#include "utils/Logger.h"

namespace backend
{

    inline void* CreateHookOnce(void* target, void* detour, void** original, const char* label)
    {
        const MH_STATUS status = MH_CreateHook(target, detour, original);
        if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED)
        {
            LOG_ERROR("MH_CreateHook({}) failed: {}", label, MH_StatusToString(status));
            return nullptr;
        }
        return target;
    }

    inline bool EnableHook(void* target, const char* label)
    {
        const MH_STATUS status = MH_EnableHook(target);
        if (status != MH_OK)
        {
            LOG_ERROR("MH_EnableHook({}) failed: {}", label, MH_StatusToString(status));
            return false;
        }
        return true;
    }

} // namespace backend

#endif // HACKFRAMEWORK_MINHOOKHELPER_H
