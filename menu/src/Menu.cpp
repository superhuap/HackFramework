//
// Created by superhuap on 2026/8/25.
//

#include "menu/Menu.h"

#include <imgui.h>
#include <imgui_impl_win32.h>

#include "utils/InputHook.h"
#include "utils/Logger.h"

#include "features/FeatureManager.h"
#include "features/Features.h"

namespace Menu
{

    bool Initialize(HWND hWnd)
    {
        if (ImGui::GetCurrentContext())
            return true;

        ImGui::CreateContext();

        if (!ImGui_ImplWin32_Init(hWnd))
        {
            LOG_ERROR("Failed to initialize ImGui Win32 backend");
            return false;
        }

        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;

        // const ImWchar* glyph_ranges = io.Fonts->GetGlyphRangesChineseFull();
        // io.Fonts->AddFontFromFileTTF(R"(c:\Windows\Fonts\msyh.ttc)", 32.0f, nullptr, glyph_ranges);

        Feature::RegisterAll();
        Feature::Manager::Get().Start();

        LOG_INFO("ImGui context initialized");
        return true;
    }

    void Render()
    {
        if (!Utils::Input::menu_visible.load())
            return;

        if (ImGui::Begin("HackFramework", nullptr,  ImGuiWindowFlags_NoSavedSettings))
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

        if (ImGui::GetIO().BackendPlatformUserData)
            ImGui_ImplWin32_Shutdown();

        ImGui::DestroyContext();

        LOG_INFO("ImGui context destroyed");
    }

} // namespace Menu