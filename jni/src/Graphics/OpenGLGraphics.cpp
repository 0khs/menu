#include <GLES3/gl3.h>
#include <android/native_window.h>
#include <android/log.h>
#include "OpenGLGraphics.h"
#include "imgui_impl_opengl3.h"

#define GL_LOG_TAG "OpenGLGraphics"
#define GL_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, GL_LOG_TAG, __VA_ARGS__)
#define GL_LOGW(...) __android_log_print(ANDROID_LOG_WARN, GL_LOG_TAG, __VA_ARGS__)

bool OpenGLGraphics::Create() {
    const EGLint egl_attributes[] = {
        EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8,
        EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 16,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE,
        EGL_WINDOW_BIT, EGL_NONE
    };

    const EGLint egl_es2_attributes[] = {
        EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8,
        EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 16,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE,
        EGL_WINDOW_BIT, EGL_NONE
    };

    m_EglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (m_EglDisplay == EGL_NO_DISPLAY) {
        GL_LOGE("eglGetDisplay failed");
        return false;
    }

    if (!eglInitialize(m_EglDisplay, nullptr, nullptr)) {
        GL_LOGE("eglInitialize failed");
        m_EglDisplay = EGL_NO_DISPLAY;
        return false;
    }

    EGLint num_configs = 0;
    EGLConfig egl_config;
    EGLBoolean choose_result = eglChooseConfig(m_EglDisplay, egl_attributes, nullptr, 0, &num_configs);
    if (!choose_result || num_configs == 0) {
        GL_LOGW("ES3 config not available, trying ES2");
        choose_result = eglChooseConfig(m_EglDisplay, egl_es2_attributes, nullptr, 0, &num_configs);
        if (!choose_result || num_configs == 0) {
            GL_LOGE("No EGL config available");
            eglTerminate(m_EglDisplay);
            m_EglDisplay = EGL_NO_DISPLAY;
            return false;
        }
        if (!eglChooseConfig(m_EglDisplay, egl_es2_attributes, &egl_config, 1, &num_configs)) {
            GL_LOGE("eglChooseConfig(ES2) failed");
            eglTerminate(m_EglDisplay);
            m_EglDisplay = EGL_NO_DISPLAY;
            return false;
        }
    } else {
        if (!eglChooseConfig(m_EglDisplay, egl_attributes, &egl_config, 1, &num_configs)) {
            GL_LOGE("eglChooseConfig(ES3) failed");
            eglTerminate(m_EglDisplay);
            m_EglDisplay = EGL_NO_DISPLAY;
            return false;
        }
    }

    EGLint egl_format;
    eglGetConfigAttrib(m_EglDisplay, egl_config, EGL_NATIVE_VISUAL_ID, &egl_format);
    ANativeWindow_setBuffersGeometry(m_Window, 0, 0, egl_format);

    EGLint client_version = 3;
    if (num_configs > 0) {
        EGLint renderable = 0;
        eglGetConfigAttrib(m_EglDisplay, egl_config, EGL_RENDERABLE_TYPE, &renderable);
        if (!(renderable & EGL_OPENGL_ES3_BIT))
            client_version = 2;
    }

    const EGLint egl_context_attributes[] = {EGL_CONTEXT_CLIENT_VERSION, client_version, EGL_NONE};
    m_EglContext = eglCreateContext(m_EglDisplay, egl_config, EGL_NO_CONTEXT, egl_context_attributes);
    if (m_EglContext == EGL_NO_CONTEXT) {
        if (client_version == 3) {
            GL_LOGW("ES3 context failed, trying ES2");
            const EGLint es2_ctx_attr[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
            m_EglContext = eglCreateContext(m_EglDisplay, egl_config, EGL_NO_CONTEXT, es2_ctx_attr);
        }
        if (m_EglContext == EGL_NO_CONTEXT) {
            GL_LOGE("eglCreateContext failed");
            eglTerminate(m_EglDisplay);
            m_EglDisplay = EGL_NO_DISPLAY;
            return false;
        }
    }

    m_EglSurface = eglCreateWindowSurface(m_EglDisplay, egl_config, m_Window, nullptr);
    if (m_EglSurface == EGL_NO_SURFACE) {
        GL_LOGE("eglCreateWindowSurface failed");
        eglDestroyContext(m_EglDisplay, m_EglContext);
        eglTerminate(m_EglDisplay);
        m_EglDisplay = EGL_NO_DISPLAY;
        m_EglContext = EGL_NO_CONTEXT;
        return false;
    }

    if (!eglMakeCurrent(m_EglDisplay, m_EglSurface, m_EglSurface, m_EglContext)) {
        GL_LOGE("eglMakeCurrent failed");
        eglDestroySurface(m_EglDisplay, m_EglSurface);
        eglDestroyContext(m_EglDisplay, m_EglContext);
        eglTerminate(m_EglDisplay);
        m_EglDisplay = EGL_NO_DISPLAY;
        m_EglSurface = EGL_NO_SURFACE;
        m_EglContext = EGL_NO_CONTEXT;
        return false;
    }

    glClearColor(0.0, 0.0, 0.0, 0.0);
    return true;
}

void OpenGLGraphics::Setup() {
    const char* glsl_version = "#version 300 es";
    GLint major = 0, minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    if (major < 3) {
        glsl_version = "#version 100";
    }
    ImGui_ImplOpenGL3_Init(glsl_version);
}

void OpenGLGraphics::PrepareFrame(bool resize) {
    ImGui_ImplOpenGL3_NewFrame();
}

void OpenGLGraphics::Render(ImDrawData *drawData) {
    if (m_EglDisplay == EGL_NO_DISPLAY || m_EglSurface == EGL_NO_SURFACE || m_EglContext == EGL_NO_CONTEXT) return;

    glViewport(0, 0, (GLsizei)m_Width, (GLsizei)m_Height);
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
        if (m_EglContext != EGL_NO_CONTEXT) {
            eglDestroyContext(m_EglDisplay, m_EglContext);
            m_EglContext = EGL_NO_CONTEXT;
        }
        if (m_EglSurface != EGL_NO_SURFACE) {
            eglDestroySurface(m_EglDisplay, m_EglSurface);
            m_EglSurface = EGL_NO_SURFACE;
        }
        eglReleaseThread();
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
    if (textureId != 0)
        glDeleteTextures(1, &textureId);
    delete tex_data;
}
