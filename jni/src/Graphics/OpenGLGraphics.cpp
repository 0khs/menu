#include <GLES3/gl3.h>
#include <android/native_window.h>
#include "OpenGLGraphics.h"
#include "imgui_impl_opengl3.h"

bool OpenGLGraphics::Create() {
    const EGLint egl_attributes[] = {EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8,
                                     EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 16,
                                     EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE,
                                     EGL_WINDOW_BIT, EGL_NONE};

    m_EglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (m_EglDisplay == EGL_NO_DISPLAY) return false;

    if (!eglInitialize(m_EglDisplay, nullptr, nullptr)) return false;

    EGLint num_configs = 0;
    if (!eglChooseConfig(m_EglDisplay, egl_attributes, nullptr, 0, &num_configs)) return false;

    EGLConfig egl_config;
    if (!eglChooseConfig(m_EglDisplay, egl_attributes, &egl_config, 1, &num_configs)) return false;

    EGLint egl_format;
    eglGetConfigAttrib(m_EglDisplay, egl_config, EGL_NATIVE_VISUAL_ID, &egl_format);
    ANativeWindow_setBuffersGeometry(m_Window, 0, 0, egl_format);

    const EGLint egl_context_attributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    m_EglContext = eglCreateContext(m_EglDisplay, egl_config, EGL_NO_CONTEXT, egl_context_attributes);
    if (m_EglContext == EGL_NO_CONTEXT) return false;

    m_EglSurface = eglCreateWindowSurface(m_EglDisplay, egl_config, m_Window, nullptr);
    if (m_EglSurface == EGL_NO_SURFACE) return false;

    if (!eglMakeCurrent(m_EglDisplay, m_EglSurface, m_EglSurface, m_EglContext)) return false;

    glClearColor(0.0, 0.0, 0.0, 0.0);
    return true;
}

void OpenGLGraphics::Setup() {
    ImGui_ImplOpenGL3_Init("#version 300 es");
}

void OpenGLGraphics::PrepareFrame(bool resize) {
    ImGui_ImplOpenGL3_NewFrame();
}

void OpenGLGraphics::Render(ImDrawData *drawData) {
    if (m_EglDisplay == EGL_NO_DISPLAY || m_EglSurface == EGL_NO_SURFACE) return;

    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(drawData);

    if (!eglSwapBuffers(m_EglDisplay, m_EglSurface)) {
        EGLint err = eglGetError();
        if (err == EGL_BAD_SURFACE || err == EGL_BAD_NATIVE_WINDOW || err == EGL_CONTEXT_LOST) {
            Cleanup();
        }
    }
}

void OpenGLGraphics::PrepareShutdown() {
    ImGui_ImplOpenGL3_Shutdown();
}

void OpenGLGraphics::Cleanup() {
    if (m_EglDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(m_EglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (m_EglContext != EGL_NO_CONTEXT) eglDestroyContext(m_EglDisplay, m_EglContext);
        if (m_EglSurface != EGL_NO_SURFACE) eglDestroySurface(m_EglDisplay, m_EglSurface);
        eglTerminate(m_EglDisplay);
    }
    m_EglDisplay = EGL_NO_DISPLAY;
    m_EglSurface = EGL_NO_SURFACE;
    m_EglContext = EGL_NO_CONTEXT;
}

BaseTexData *OpenGLGraphics::LoadTexture(BaseTexData *tex, void *pixel_data) {
    auto tex_data = new OpenglTextureData();
    tex_data->Width = tex->Width;
    tex_data->Height = tex->Height;
    tex_data->Channels = tex->Channels;

    GLuint textureId;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_data->Width, tex_data->Height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixel_data);
    tex_data->DS = (void *) (intptr_t) textureId;
    return tex_data;
}

void OpenGLGraphics::RemoveTexture(BaseTexData *tex) {
    auto tex_data = (OpenglTextureData *) tex;
    auto textureId = (GLuint) (intptr_t) tex_data->DS;
    glDeleteTextures(1, &textureId);
    delete tex_data;
}
