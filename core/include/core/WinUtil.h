//
// Created by superhuap on 2026/11/18.
// Adapted from Universal-Dear-ImGui-Hook (MIT), src/windows/platform.h and dxgi.cpp.
//

#ifndef HACKFRAMEWORK_WINUTIL_H
#define HACKFRAMEWORK_WINUTIL_H

#include <windows.h>

namespace Core
{

    // COM release helper: releases `object` and nulls the pointer.
    template <typename T>
    void Release(T*& object)
    {
        if (object)
        {
            object->Release();
            object = nullptr;
        }
    }

    // Resolves a virtual method from an object's vtable.
    inline void* VirtualMethod(void* object, size_t index)
    {
        return (*reinterpret_cast<void***>(object))[index];
    }

    // Typed GetProcAddress.
    template <typename Fn>
    Fn GetProc(HMODULE module, const char* name)
    {
        return reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(module, name)));
    }

    // Hidden 64x64 window for dummy swap chains ("STATIC" is a system class).
    HWND CreateHiddenWindow();

} // namespace Core

#endif // HACKFRAMEWORK_WINUTIL_H
