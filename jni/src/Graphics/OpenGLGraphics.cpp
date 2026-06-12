#include "OpenGLGraphics.h"

#include <dlfcn.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include "dobby.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "Menu.h"
#include "Logger.h"
#include "CPU.h"
#include "TouchHelperA.h"

namespace OpenGLGraphics {

static bool s_imguiInit = false;
static bool s_active = false;
static int s_w = 0, s_h = 0;

static EGLBoolean (*orig_eglSwapBuffers)(EGLDisplay, EGLSurface) = nullptr;

static EGLBoolean hooked_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    eglQuerySurface(dpy, surface, EGL_WIDTH, &s_w);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &s_h);

    if (s_w <= 0 || s_h <= 0) {
        return orig_eglSwapBuffers(dpy, surface);
    }

    if (!s_imguiInit) {
        const char* glVersion = (const char*)glGetString(GL_VERSION);
        LOGI("OpenGLGraphics: GL_VERSION = %s", glVersion ? glVersion : "null");

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2((float)s_w, (float)s_h);

        if (!ImGui_ImplOpenGL3_Init("#version 300 es")) {
            LOGE("OpenGLGraphics: ImGui_ImplOpenGL3_Init failed");
        }

        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            LOGE("OpenGLGraphics: GL error after init: 0x%x", err);
        }

        Touch::Init({(float)s_w, (float)s_h}, false);
        InitMenuStyle();

        s_imguiInit = true;
        s_active = true;
        LOGI("OpenGLGraphics: ImGui init done %dx%d", s_w, s_h);
    }

    CPU::Tick();

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)s_w, (float)s_h);
    io.DeltaTime = 1.0f / 60.0f;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    DrawMenu();

    Touch::SetMenuBounds(LastCoordinate.Pos_x, LastCoordinate.Pos_y, LastCoordinate.Size_x, LastCoordinate.Size_y);

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    return orig_eglSwapBuffers(dpy, surface);
}

bool TryHook() {
    void* egl = dlopen("libEGL.so", RTLD_NOW);
    if (!egl) {
        LOGE("OpenGLGraphics: dlopen libEGL.so failed");
        return false;
    }

    void* addr = dlsym(egl, "eglSwapBuffers");
    if (!addr) {
        LOGE("OpenGLGraphics: dlsym eglSwapBuffers failed");
        return false;
    }

    int res = DobbyHook(addr, (void*)hooked_eglSwapBuffers, (void**)&orig_eglSwapBuffers);
    if (res != 0) {
        LOGE("OpenGLGraphics: DobbyHook eglSwapBuffers failed, code %d", res);
        return false;
    }

    LOGI("OpenGLGraphics: eglSwapBuffers hooked");
    return true;
}

bool IsActive() {
    return s_active;
}

}
