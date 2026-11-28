//
// Created by superhuap on 2026/11/15.
//

#ifndef HACKFRAMEWORK_HOOKHELPER_H
#define HACKFRAMEWORK_HOOKHELPER_H

#include <vector>

namespace HookHelper{
    bool InstallHook(const char* name, void* target, void* detour, void** original);
    bool UninstallHook(const char* name, void* target);
    bool DisableHook(const char* name, void* target);
    bool EnableHook(const char* name, void* target);

    bool EnableAllHooks();
    bool DisableAllHooks();
    bool UninstallAllHooks();

    // Removes every hook recorded in `hooks` and empties the list. Used on an
    // Initialize() failure, before the module that owns the detours unloads:
    // a hook left installed would jump into freed code.
    template <typename Entry>
    void UninstallAll(std::vector<Entry>& hooks)
    {
        for (const Entry& hook : hooks)
            UninstallHook(hook.name, hook.target);
        hooks.clear();
    }
}

#endif //HACKFRAMEWORK_HOOKHELPER_H
