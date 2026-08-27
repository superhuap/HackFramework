//
// Created by superhuap on 2026/8/25.
//

#include "utils/InputHook.h"
#include "utils/DllHelper.h"

LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Utils::Input
{

    namespace
    {

        HWND g_window = nullptr;
        WNDPROC g_originalWndProc = nullptr;
        RehookHandler g_rehookHandler = nullptr;
        UnloadHandler g_unloadHandler = nullptr;

        DWORD WINAPI ReinitializeGraphicalHooks(LPVOID lpParam)
        {
            if (g_rehookHandler)
                g_rehookHandler(reinterpret_cast<HWND>(lpParam));
            return 0;
        }

        void TriggerRehook(HWND staleWindow)
        {
            HANDLE hThread = CreateThread(nullptr, 0, ReinitializeGraphicalHooks,
                                          reinterpret_cast<LPVOID>(staleWindow), 0, nullptr);
            if (hThread)
                CloseHandle(hThread);
        }

        LRESULT WINAPI WndProc(const HWND hWnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
        {
            if (uMsg == WM_KEYDOWN)
            {
                if (wParam == VK_INSERT)
                {
                    menu_visible.store(!menu_visible.load());
                    return 0;
                }
                if (wParam == VK_HOME)
                {
                    TriggerRehook(nullptr);
                    return 0;
                }
                if (wParam == VK_END)
                {
                    shutting_down.store(true);
                    if (g_unloadHandler)
                        g_unloadHandler();
                    Utils::UnloadDLL();
                    return 0;
                }
            }
            else if (uMsg == WM_DESTROY)
            {
                TriggerRehook(hWnd);
            }

            if (menu_visible.load())
                ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

            return CallWindowProc(g_originalWndProc, hWnd, uMsg, wParam, lParam);
        }

    } // namespace

    void Install(HWND hWnd, RehookHandler rehookHandler, UnloadHandler unloadHandler)
    {
        g_window = hWnd;
        g_rehookHandler = rehookHandler;
        g_unloadHandler = unloadHandler;
        g_originalWndProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
    }

    void Remove()
    {
        if (g_originalWndProc && g_window)
            SetWindowLongPtrW(g_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_originalWndProc));
        g_originalWndProc = nullptr;
        g_rehookHandler = nullptr;
        g_unloadHandler = nullptr;
        g_window = nullptr;
    }

} // namespace Utils::Input
