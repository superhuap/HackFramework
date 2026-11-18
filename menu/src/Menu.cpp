//
// Created by superhuap on 2026/11/18.
// Ported from the previous HackFramework iteration.
//

#include "menu/Menu.h"

#include <imgui.h>
#include <imgui_impl_win32.h>

#include "utils/InputHook.h"
#include "utils/InputLock.h"
#include "utils/Logger.h"
#include "feature/core/FeatureManager.h"
#include "feature/impl/Features.h"

namespace Menu
{

    namespace
    {
        HWND g_window = nullptr;
    }

    bool Initialize(HWND hWnd)
    {
        if (!hWnd)
            return false;

        if (ImGui::GetCurrentContext())
        {
            // Rebind to a new window (the game recreated its swap chain).
            if (hWnd == g_window)
                return true;
            {
                // The window thread may still be inside the stale WndProc:
                // drop the subclass under the lock it also takes.
                InputLock::Scoped lock;
                InputHook::Remove();
            }
            if (ImGui::GetIO().BackendPlatformUserData)
                ImGui_ImplWin32_Shutdown();
            if (!ImGui_ImplWin32_Init(hWnd))
            {
                Logger::Error("[Menu] Initialize: ImGui Win32 rebind failed");
                return false;
            }
            g_window = hWnd;
            InputHook::Install(hWnd);
            Logger::Debug("[Menu] Initialize: rebound to window {:p}", reinterpret_cast<void*>(hWnd));
            return true;
        }

        ImGui::CreateContext();

        if (!ImGui_ImplWin32_Init(hWnd))
        {
            Logger::Error("[Menu] Initialize: ImGui Win32 backend failed");
            return false;
        }
        g_window = hWnd;
        // Subclass the window: feed ImGui and watch the hotkeys (INSERT, END).
        InputHook::Install(hWnd);

        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;

        // const ImWchar* glyph_ranges = io.Fonts->GetGlyphRangesChineseFull();
        // io.Fonts->AddFontFromFileTTF(R"(c:\Windows\Fonts\msyh.ttc)", 32.0f, nullptr, glyph_ranges);

        Feature::RegisterAll();
        Feature::Manager::Get().Start();

        Logger::Debug("[Menu] Initialize: ImGui context created");
        return true;
    }

    void Render()
    {
        if (!InputHook::menu_visible.load())
            return;

        if (ImGui::Begin("HackFramework", nullptr, ImGuiWindowFlags_NoSavedSettings))
        {
            Feature::Manager::Get().DrawMenu();
        }
        ImGui::End();

        Feature::Manager::Get().TickDraw();
    }

    void Shutdown()
    {
        Feature::Manager::Get().Stop();

        if (!ImGui::GetCurrentContext())
            return;

        {
            // No window thread may be inside our WndProc when the ImGui
            // context goes away: drop the subclass under the lock first.
            InputLock::Scoped lock;
            InputHook::Remove();
        }
        if (ImGui::GetIO().BackendPlatformUserData)
            ImGui_ImplWin32_Shutdown();

        ImGui::DestroyContext();
        g_window = nullptr;

        Logger::Debug("[Menu] Shutdown: ImGui context destroyed");
    }

} // namespace Menu
