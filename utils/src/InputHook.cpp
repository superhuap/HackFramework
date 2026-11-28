//
// Created by superhuap on 2026/11/18.
// Adapted from the previous HackFramework iteration.
//

#include "utils/InputHook.h"
#include "utils/DllHelper.h"
#include "utils/InputLock.h"
#include "utils/Logger.h"

#include <chrono>
#include <thread>

// Forward declaration copied from imgui_impl_win32.h (imgui declares it in a
// `#if 0` block on purpose, to avoid dragging <windows.h> into that header).
LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace InputHook
{

    namespace
    {

        HWND g_window = nullptr;
        // Written by Install()/Remove() and read at the tail of an in-flight
        // WndProc: an atomic snapshot is what keeps that tail from calling a
        // null pointer after Remove() cleared it.
        std::atomic<WNDPROC> g_originalWndProc{nullptr};
        RehookHandler g_rehookHandler = nullptr;
        UnloadHandler g_unloadHandler = nullptr;

        // Threads currently executing inside this DLL through the subclassed
        // window procedure. The unload waits for it to drain before freeing
        // the library: a thread returning into freed code would crash.
        std::atomic<int> g_inWndProc{0};

        class WndProcGuard
        {
        public:
            WndProcGuard() { ++g_inWndProc; }
            ~WndProcGuard() { --g_inWndProc; }
        };

        // A named function rather than a lambda: on 32-bit targets the thread
        // entry point must use the WINAPI (__stdcall) calling convention, which
        // an ordinary lambda's operator() does not have.
        DWORD WINAPI UnloadThread(LPVOID)
        {
            Remove();
            const auto deadline =
                std::chrono::steady_clock::now() + std::chrono::seconds(1);
            while (g_inWndProc > 0 && std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (g_inWndProc > 0)
                Logger::Error("[Input] Unload: threads still inside the DLL");
            if (g_unloadHandler)
                g_unloadHandler();
            FreeLibraryAndExitThread(DllHelper::GetCurrentImageBase(), 0);
            return 0;
        }

        LRESULT WINAPI WndProc(const HWND hWnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
        {
            WndProcGuard guard;
            // Remove() may clear the member while this call is still in flight,
            // so the tail chains to a snapshot taken on entry.
            const WNDPROC original = g_originalWndProc.load();

            if (uMsg == WM_KEYDOWN)
            {
                if (wParam == VK_INSERT)
                {
                    menu_visible.store(!menu_visible.load());
                    return 0;
                }
                if (wParam == VK_END && !shutting_down.exchange(true))
                {
                    Logger::Debug("[Input] Unload: requested");
                    // Signal only: the window thread must leave this DLL before
                    // the library is freed, so the unload runs on its own
                    // thread (which first removes the subclass).
                    HANDLE hThread = CreateThread(nullptr, 0, UnloadThread, nullptr, 0, nullptr);
                    if (hThread)
                        CloseHandle(hThread);
                    return 0;
                }
            }
            else if (uMsg == WM_DESTROY)
            {
                Logger::Debug("[Input] Rehook: window destroyed");
                if (g_rehookHandler)
                    g_rehookHandler(hWnd);
            }

            if (!shutting_down.load() && menu_visible.load())
            {
                InputLock::Scoped lock;
                ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
            }

            if (original)
                return CallWindowProc(original, hWnd, uMsg, wParam, lParam);
            // Unsubclassed while this message was in flight: there is no
            // original procedure left to chain to, so fall back to the default.
            return IsWindowUnicode(hWnd) ? DefWindowProcW(hWnd, uMsg, wParam, lParam)
                                         : DefWindowProcA(hWnd, uMsg, wParam, lParam);
        }

    } // namespace

    void SetHandlers(RehookHandler rehookHandler, UnloadHandler unloadHandler)
    {
        g_rehookHandler = rehookHandler;
        g_unloadHandler = unloadHandler;
    }

    void Install(HWND hWnd)
    {
        if (!hWnd || g_originalWndProc.load())
            return;
        g_window = hWnd;
        g_originalWndProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
        Logger::Debug("[Input] Install: window {:p} subclassed", reinterpret_cast<void*>(hWnd));
    }

    void Remove()
    {
        const WNDPROC original = g_originalWndProc.load();
        if (original && g_window)
            SetWindowLongPtrW(g_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original));
        g_originalWndProc = nullptr;
        g_window = nullptr;
    }

} // namespace InputHook
