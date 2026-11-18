//
// Created by superhuap on 2026/11/15.
//

#ifndef HACKFRAMEWORK_HOOKHELPER_H
#define HACKFRAMEWORK_HOOKHELPER_H

namespace HookHelper{
    bool InstallHook(const char* name, void* target, void* detour, void** original);
    bool UninstallHook(const char* name, void* target);
    bool DisableHook(const char* name, void* target);
    bool EnableHook(const char* name, void* target);

    bool EnableAllHooks();
    bool DisableAllHooks();
    bool UninstallAllHooks();
}

#endif //HACKFRAMEWORK_HOOKHELPER_H
