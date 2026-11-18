//
// Created by superhuap on 2026/11/18.
// Adapted from Universal-Dear-ImGui-Hook (MIT), src/gl_state.h
// (OverlayGlState; the capture helper is not ported).
// Header-only: the state Dear ImGui's OpenGL backend changes while it draws
// and that must be set back when a fault interrupts it, as in a draw callback
// of the application. Uses the loader of the Dear ImGui backend, which its
// NewFrame() loads.
//

#ifndef HACKFRAMEWORK_GLSTATE_H
#define HACKFRAMEWORK_GLSTATE_H

#include <imgui_impl_opengl3_loader.h>

namespace GlOverlay
{

    // - a framebuffer object: the overlay goes to the window's back buffer;
    // - GL_FRAMEBUFFER_SRGB, which would encode the overlay's colors, already
    //   in sRGB, a second time;
    // - a pixel unpack buffer, which texture uploads would read instead of the
    //   overlay's pixels.
    class Scoped
    {
    public:
        // Not cached: the loader may unload the library on shutdown and load
        // it at another address on the next start.
        Scoped()
            : bindFramebuffer_(reinterpret_cast<void(APIENTRYP)(GLenum, GLuint)>(imgl3wGetProcAddress("glBindFramebuffer")))
        {
            if (bindFramebuffer_)
            {
                glGetIntegerv(kDrawFramebufferBinding, &drawFramebuffer_);
                bindFramebuffer_(kDrawFramebuffer, 0);
            }
            glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer_);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            srgb_ = glIsEnabled(kFramebufferSrgb);
            glDisable(kFramebufferSrgb);
        }

        ~Scoped()
        {
            if (srgb_)
                glEnable(kFramebufferSrgb);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(unpackBuffer_));
            if (bindFramebuffer_)
                bindFramebuffer_(kDrawFramebuffer, static_cast<GLuint>(drawFramebuffer_));
        }

        Scoped(const Scoped&) = delete;
        Scoped& operator=(const Scoped&) = delete;

    private:
        static constexpr GLenum kDrawFramebuffer = 0x8CA9;
        static constexpr GLenum kDrawFramebufferBinding = 0x8CA6;
        static constexpr GLenum kFramebufferSrgb = 0x8DB9;
        void(APIENTRYP bindFramebuffer_)(GLenum, GLuint);
        GLint drawFramebuffer_ = 0;
        GLint unpackBuffer_ = 0;
        GLboolean srgb_ = GL_FALSE;
    };

} // namespace GlOverlay

#endif // HACKFRAMEWORK_GLSTATE_H
