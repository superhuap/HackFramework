//
// Created by superhuap on 2026/8/25.
//

#include "backend/OPENGL_Backend.h"
#include "backend/MinHookHelper.h"

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>

#include "menu/Menu.h"
#include "utils/InputHook.h"
#include "utils/Logger.h"

OPENGL_Backend* OPENGL_Backend::s_instance = nullptr;

namespace
{

    BOOL(WINAPI * oWglSwapBuffers)(HDC) = nullptr;

} // namespace

bool OPENGL_Backend::Initialize(HWND hWnd)
{
    s_instance = this;

    if (!Menu::Initialize(hWnd))
        return false;

    HMODULE openGL32 = GetModuleHandleA("opengl32.dll");
    if (!openGL32)
    {
        LOG_ERROR("Failed to get opengl32.dll handle");
        return false;
    }
    LOG_INFO("OpenGL32: ImageBase: {}", reinterpret_cast<void*>(openGL32));

    void* fnWglSwapBuffers = reinterpret_cast<void*>(GetProcAddress(openGL32, "wglSwapBuffers"));
    if (!fnWglSwapBuffers)
    {
        LOG_ERROR("Failed to get wglSwapBuffers address");
        return false;
    }
    LOG_INFO("OpenGL32: fnWglSwapBuffers: {}", fnWglSwapBuffers);

    if (!backend::CreateHookOnce(fnWglSwapBuffers, &Hook_wglSwapBuffers,
                                 reinterpret_cast<void**>(&oWglSwapBuffers), "wglSwapBuffers"))
        return false;
    if (!backend::EnableHook(fnWglSwapBuffers, "wglSwapBuffers"))
        return false;

    LOG_INFO("OpenGL Backend initialized successfully");
    return true;
}

void OPENGL_Backend::Shutdown()
{
    LOG_INFO("OpenGL Backend shutting down...");

    if (ImGui::GetCurrentContext())
    {
        if (ImGui::GetIO().BackendRendererUserData)
            ImGui_ImplOpenGL3_Shutdown();
    }

    Menu::Shutdown();

    s_instance = nullptr;
    LOG_INFO("OpenGL Backend shutdown complete");
}

BOOL WINAPI OPENGL_Backend::Hook_wglSwapBuffers(HDC hdc)
{
    if (!Utils::Input::shutting_down.load() && s_instance && ImGui::GetCurrentContext())
    {
        if (!ImGui::GetIO().BackendRendererUserData)
            ImGui_ImplOpenGL3_Init();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        Menu::Render();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    return oWglSwapBuffers(hdc);
}
