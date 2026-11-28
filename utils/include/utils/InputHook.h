//
// Created by superhuap on 2026/11/18.
// Adapted from the previous HackFramework iteration.
//

#ifndef HACKFRAMEWORK_INPUTHOOK_H
#define HACKFRAMEWORK_INPUTHOOK_H

#include <windows.h>
#include <atomic>

namespace InputHook
{

    inline std::atomic<bool> menu_visible{true};
    inline std::atomic<bool> shutting_down{false};

    using RehookHandler = void (*)(HWND staleWindow);
    // Returns false when the library must stay loaded: a hook callback did not
    // drain in time, so freeing the module would free code a thread still runs.
    using UnloadHandler = bool (*)();

    // Register the handlers before the hooks bind a window. The rehook handler
    // may be null: the overlay rebinds itself at the next present of a new
    // swap chain, no manual rehook is needed.
    void SetHandlers(RehookHandler rehookHandler, UnloadHandler unloadHandler);

    // Subclasses `hWnd` to feed ImGui and to watch the hotkeys. Called once
    // the overlay knows the window of the game, from the rendering path.
    void Install(HWND hWnd);

    void Remove();

} // namespace InputHook

#endif // HACKFRAMEWORK_INPUTHOOK_H
