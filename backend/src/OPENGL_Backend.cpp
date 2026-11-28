//
// Created by superhuap on 2026/11/18.
// Adapted from Universal-Dear-ImGui-Hook (MIT), src/windows/opengl.cpp,
// src/gl_state.h and src/uih.cpp (AbandonRenderer; the capture paths are not
// ported).
//

#include "backend/OPENGL_Backend.h"

#include <atomic>
#include <mutex>
#include <vector>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_opengl3_loader.h>
#include <imgui_impl_win32.h>

#include "backend/GlState.h"
#include "menu/Menu.h"
#include "utils/HookHelper.h"
#include "utils/InputHook.h"
#include "utils/InputLock.h"
#include "utils/Logger.h"

#include "core/Guarded.h"
#include "core/ScopedCall.h"
#include "core/Teardown.h"
#include "core/WinUtil.h"

namespace
{

    using WglSwapBuffersFn = BOOL(__stdcall*)(HDC);
    using WglGetCurrentContextFn = HGLRC(__stdcall*)();
    using WglDeleteContextFn = BOOL(__stdcall*)(HGLRC);

    // Presents to wait before following an application that renders with
    // another context.
    constexpr int kContextSwitchFrames = 60;

    HMODULE g_module = nullptr;
    WglSwapBuffersFn g_swapBuffers = nullptr;
    WglGetCurrentContextFn g_getCurrentContext = nullptr;
    WglDeleteContextFn g_deleteContext = nullptr;
    HGLRC g_context = nullptr;
    HWND g_boundWindow = nullptr;
    int g_contextMisses = 0;
    bool g_ready = false;

    // One overlay thread at a time.
    std::mutex g_frameMutex;

    struct HookEntry
    {
        const char* name;
        void* target;
    };
    std::vector<HookEntry> g_installedHooks;

    void Teardown();

    // State the Dear ImGui backend changes while it draws and restores
    // afterwards, set back here when a fault interrupts it, as in a draw
    // callback of the application.
    class BackendGlState
    {
    public:
        BackendGlState()
        {
            glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture_);
            glActiveTexture(GL_TEXTURE0);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture_);
            glActiveTexture(static_cast<GLenum>(activeTexture_));
            glGetIntegerv(GL_CURRENT_PROGRAM, &program_);
            glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer_);
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray_);
            glGetIntegerv(GL_VIEWPORT, viewport_);
            glGetIntegerv(GL_SCISSOR_BOX, scissorBox_);
            for (size_t i = 0; i < kBlendCount; ++i)
                glGetIntegerv(kBlend[i], &blend_[i]);
            for (size_t i = 0; i < kCapabilityCount; ++i)
                enabled_[i] = glIsEnabled(kCapabilities[i]);
        }

        void Restore() const
        {
            glUseProgram(static_cast<GLuint>(program_));
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture_));
            glActiveTexture(static_cast<GLenum>(activeTexture_));
            glBindVertexArray(static_cast<GLuint>(vertexArray_));
            glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(arrayBuffer_));
            glBlendEquationSeparate(static_cast<GLenum>(blend_[0]), static_cast<GLenum>(blend_[1]));
            glBlendFuncSeparate(static_cast<GLenum>(blend_[2]), static_cast<GLenum>(blend_[3]),
                                static_cast<GLenum>(blend_[4]), static_cast<GLenum>(blend_[5]));
            for (size_t i = 0; i < kCapabilityCount; ++i)
                enabled_[i] ? glEnable(kCapabilities[i]) : glDisable(kCapabilities[i]);
            glViewport(viewport_[0], viewport_[1], viewport_[2], viewport_[3]);
            glScissor(scissorBox_[0], scissorBox_[1], scissorBox_[2], scissorBox_[3]);
        }

    private:
        static constexpr GLenum kBlend[] = {GL_BLEND_EQUATION_RGB, GL_BLEND_EQUATION_ALPHA, GL_BLEND_SRC_RGB,
                                            GL_BLEND_DST_RGB,     GL_BLEND_SRC_ALPHA,      GL_BLEND_DST_ALPHA};
        static constexpr size_t kBlendCount = sizeof(kBlend) / sizeof(kBlend[0]);
        static constexpr GLenum kCapabilities[] = {GL_BLEND, GL_CULL_FACE, GL_DEPTH_TEST, GL_STENCIL_TEST,
                                                   GL_SCISSOR_TEST};
        static constexpr size_t kCapabilityCount = sizeof(kCapabilities) / sizeof(kCapabilities[0]);
        GLint activeTexture_ = 0;
        GLint texture_ = 0;
        GLint program_ = 0;
        GLint arrayBuffer_ = 0;
        GLint vertexArray_ = 0;
        GLint viewport_[4] = {};
        GLint scissorBox_[4] = {};
        GLint blend_[kBlendCount] = {};
        GLboolean enabled_[kCapabilityCount] = {};
    };

    // The renderer of the overlay is gone without its context: the textures
    // and the backend data are marked as the shutdown would leave them, so the
    // next backend starts fresh. Object names belong to a context: deleting
    // them in another one would delete objects of the application.
    void AbandonRenderer()
    {
        ImGuiIO& io = ImGui::GetIO();
        ImGuiPlatformIO& platformIo = ImGui::GetPlatformIO();
        // As the backend's shutdown leaves them: created again by the next backend.
        for (ImTextureData* texture : platformIo.Textures)
        {
            if (texture->RefCount == 1)
            {
                texture->SetTexID(ImTextureID_Invalid);
                texture->SetStatus(ImTextureStatus_Destroyed);
            }
        }
        io.BackendRendererName = nullptr;
        io.BackendRendererUserData = nullptr;
        io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures);
        platformIo.ClearRendererHandlers();
    }

    // Releases the renderer and the ImGui context. Runs on the rendering
    // thread, from ServiceTeardown(); idempotent.
    void Teardown()
    {
        if (!g_ready)
            return;
        Logger::Debug("[OpenGL] Teardown: releasing the overlay");
        // Object names belong to a context: deleting them in another one would
        // delete objects of the application.
        if (g_getCurrentContext() == g_context)
        {
            if (ImGui::GetCurrentContext())
                ImGui_ImplOpenGL3_Shutdown();
            Menu::Shutdown();
        }
        else if (ImGui::GetCurrentContext())
        {
            AbandonRenderer();
        }
        g_ready = false;
        g_context = nullptr;
        g_boundWindow = nullptr;
        g_contextMisses = 0;
    }

    bool Init(HWND window, HGLRC context)
    {
        if (!Menu::Initialize(window))
            return false;
        if (!ImGui_ImplOpenGL3_Init())
        {
            Logger::Error("[OpenGL] Init: backend initialization failed");
            Menu::Shutdown();
            return false;
        }
        g_context = context;
        g_boundWindow = window;
        g_ready = true;
        Logger::Debug("[OpenGL] Init: bound to context {:p}, window {:p}", reinterpret_cast<void*>(context),
                      reinterpret_cast<void*>(window));
        return true;
    }

    // ImGui frame: renderer new frame, menu between NewFrame() and Render()
    // under the input lock, then the draw data to render.
    ImDrawData* Frame()
    {
        InputLock::Scoped lock;
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        Menu::Render();

        ImGui::Render();
        return ImGui::GetDrawData();
    }

    void Render(HDC hdc)
    {
        HGLRC context = g_getCurrentContext();
        HWND window = WindowFromDC(hdc);
        if (!context || !window)
            return;
        if (g_ready && context != g_context)
        {
            if (++g_contextMisses < kContextSwitchFrames)
                return;
            Teardown();
        }
        if (!g_ready && !Init(window, context))
            return;
        g_contextMisses = 0;
        if (!Menu::Initialize(window))
            return;

        ImGui_ImplOpenGL3_NewFrame();
        ImDrawData* drawData = Frame();

        // The framebuffer and pixel unpack state of the application, set
        // around the drawing of the overlay.
        GlOverlay::Scoped overlay;
        BackendGlState backend;
        if (!Core::Guarded([&] { ImGui_ImplOpenGL3_RenderDrawData(drawData); }))
            backend.Restore();
    }

    bool MayRender()
    {
        return !InputHook::shutting_down.load() && !Core::Faulted() && (!g_boundWindow || IsWindow(g_boundWindow));
    }

    BOOL WINAPI HookSwapBuffers(HDC hdc)
    {
        Core::ScopedCall call;
        if (MayRender())
        {
            if (g_frameMutex.try_lock())
            {
                Core::Guarded([&] { Render(hdc); });
                Core::ServiceTeardown(&Teardown);
                g_frameMutex.unlock();
            }
        }
        else
        {
            Core::ServiceTeardown(&Teardown);
        }
        return g_swapBuffers(hdc);
    }

    BOOL WINAPI HookDeleteContext(HGLRC context)
    {
        Core::ScopedCall call;
        // A new context may get the same handle: the overlay must not take it for its own.
        if (context && context == g_context && !InputHook::shutting_down.load())
            Core::Guarded(&Teardown);
        return g_deleteContext(context);
    }

} // namespace

bool OPENGL_Backend::Initialize()
{
    Logger::Debug("[OpenGL] Install: starting");

    Core::ResetFault();
    InputHook::shutting_down.store(false);

    g_module = LoadLibraryW(L"opengl32.dll");
    g_getCurrentContext = Core::GetProc<WglGetCurrentContextFn>(g_module, "wglGetCurrentContext");
    void* swapBuffers = Core::GetProc<void*>(g_module, "wglSwapBuffers");
    void* deleteContext = Core::GetProc<void*>(g_module, "wglDeleteContext");
    if (!g_getCurrentContext || !swapBuffers || !deleteContext)
    {
        Logger::Error("[OpenGL] Install: opengl32.dll not loadable");
        if (g_module)
        {
            FreeLibrary(g_module);
            g_module = nullptr;
        }
        return false;
    }

    if (!HookHelper::InstallHook("wglDeleteContext", deleteContext,
                                 reinterpret_cast<void*>(&HookDeleteContext),
                                 reinterpret_cast<void**>(&g_deleteContext)))
    {
        Logger::Critical("[OpenGL] Install: wglDeleteContext hook failed");
        if (g_module)
        {
            FreeLibrary(g_module);
            g_module = nullptr;
        }
        return false;
    }
    g_installedHooks.push_back({"wglDeleteContext", deleteContext});

    if (!HookHelper::InstallHook("wglSwapBuffers", swapBuffers, reinterpret_cast<void*>(&HookSwapBuffers),
                                 reinterpret_cast<void**>(&g_swapBuffers)))
    {
        // Roll back the hook that did install: Initialize() returns false and
        // the caller unloads the DLL, so a hook left pointing into it would
        // jump into freed code.
        Logger::Critical("[OpenGL] Install: wglSwapBuffers hook failed");
        for (const HookEntry& hook : g_installedHooks)
            HookHelper::UninstallHook(hook.name, hook.target);
        g_installedHooks.clear();
        if (g_module)
        {
            FreeLibrary(g_module);
            g_module = nullptr;
        }
        return false;
    }
    g_installedHooks.push_back({"wglSwapBuffers", swapBuffers});

    Logger::Debug("[OpenGL] Install: {} hook(s) active", g_installedHooks.size());
    return true;
}

void OPENGL_Backend::Shutdown()
{
    static std::atomic<bool> g_shutdown{false};
    if (g_shutdown.exchange(true))
        return;
    Logger::Debug("[OpenGL] Shutdown: stopping");

    if (!g_installedHooks.empty())
    {
        // The renderer goes first, on the rendering thread, at its next swap;
        // then the hooks, once none of them is running.
        Core::TeardownOnRenderThread(&Teardown, 1000);
        for (const HookEntry& hook : g_installedHooks)
            HookHelper::DisableHook(hook.name, hook.target);
        if (!Core::WaitForIdleHooks(2000))
        {
            // A detour is still running: removing its trampoline would free
            // code a thread is executing. Leave the hooks disabled; the unload
            // path keeps the library loaded until they drain.
            Logger::Error("[OpenGL] Shutdown: hooks still running, left disabled");
            return;
        }
        for (const HookEntry& hook : g_installedHooks)
            HookHelper::UninstallHook(hook.name, hook.target);
        g_installedHooks.clear();
    }

    // The rendering thread did not answer (the game stopped swapping, say):
    // release inline now that no hook can run.
    if (Core::TeardownPending() || g_ready)
        Core::Guarded(&Teardown);
    Core::ResetTeardown();

    if (g_module)
    {
        FreeLibrary(g_module);
        g_module = nullptr;
    }

    Logger::Debug("[OpenGL] Shutdown: done");
}
