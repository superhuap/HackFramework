//
// Created by superhuap on 2026/11/15.
//

#include "utils/HookHelper.h"

#include "MinHook.h"
#include "utils/Logger.h"

namespace HookHelper{

    bool InstallHook(const char* name, void* target, void* detour, void** original) {
        MH_STATUS status = MH_CreateHook(target, detour, original);
        if (status == MH_OK)
            status = MH_EnableHook(target);
        if (status != MH_OK) {
            Logger::Error("Hooking {} at {:p} failed: {}", name, target, MH_StatusToString(status));
            return false;
        }
        Logger::Debug("Hooked {} at {:p}", name, target);
        return true;
    }

    bool UninstallHook(const char* name, void* target) {
        MH_STATUS status = MH_DisableHook(target);
        if (status == MH_OK)
            status = MH_RemoveHook(target);
        if (status != MH_OK) {
            Logger::Error("Unhooking {} at {:p} failed: {}", name, target, MH_StatusToString(status));
            return false;
        }
        Logger::Debug("Unhooked {} at {:p}", name, target);
        return true;
    }

    bool DisableHook(const char* name, void* target) {
        MH_STATUS status = MH_DisableHook(target);
        if (status != MH_OK) {
            Logger::Error("Disabling {} failed: {}", name, MH_StatusToString(status));
            return false;
        }
        Logger::Debug("Disabled {}", name);
        return true;
    }

    bool EnableHook(const char* name, void* target) {
        MH_STATUS status = MH_EnableHook(target);
        if (status != MH_OK) {
            Logger::Error("Enabling {} failed: {}", name, MH_StatusToString(status));
            return false;
        }
        Logger::Debug("Enabled {}", name);
        return true;
    }

    bool EnableAllHooks() {
        MH_STATUS status = MH_EnableHook(MH_ALL_HOOKS);
        if (status != MH_OK) {
            Logger::Error("Enabling all hooks failed: {}", MH_StatusToString(status));
            return false;
        }
        Logger::Debug("Enabled all hooks");
        return true;
    }

    bool DisableAllHooks() {
        MH_STATUS status = MH_DisableHook(MH_ALL_HOOKS);
        if (status != MH_OK) {
            Logger::Error("Disabling all hooks failed: {}", MH_StatusToString(status));
            return false;
        }
        Logger::Debug("Disabled all hooks");
        return true;
    }

    bool UninstallAllHooks() {
        MH_STATUS status = MH_RemoveHook(MH_ALL_HOOKS);
        if (status != MH_OK) {
            Logger::Error("Unhooking all failed: {}", MH_StatusToString(status));
            return false;
        }
        Logger::Debug("Unhooked all");
        return true;
    }
}
