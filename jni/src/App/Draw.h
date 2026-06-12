#pragma once

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"

#include "TouchHelperA.h"
#include "CPU.h"
#include "Menu.h"
#include "Logger.h"

static bool s_imgui_init = false;
static int  s_gl_w = 0, s_gl_h = 0;

static EGLBoolean (*orig_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);

static EGLBoolean hooked_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    static int call_count = 0;
    if (call_count++ < 5) LOGI("hooked_eglSwapBuffers call #%d", call_count);

    eglQuerySurface(dpy, surface, EGL_WIDTH,  &s_gl_w);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &s_gl_h);

    if (!s_imgui_init) {
        LOGI("ImGui init start, size %dx%d", s_gl_w, s_gl_h);
        ImGui::CreateContext();
        ImGuiIO& io    = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2((float)s_gl_w, (float)s_gl_h);
        ImGui_ImplOpenGL3_Init("#version 300 es");
        Touch::Init({(float)s_gl_w, (float)s_gl_h}, false);
        InitMenuStyle();
        s_imgui_init = true;
        LOGI("ImGui init done");
    }

    CPU::Tick();

    ImGuiIO& io    = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)s_gl_w, (float)s_gl_h);
    io.DeltaTime   = 1.0f / 60.0f;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    DrawMenu();

    Touch::SetMenuBounds(
        LastCoordinate.Pos_x,
        LastCoordinate.Pos_y,
        LastCoordinate.Size_x,
        LastCoordinate.Size_y);

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    return orig_eglSwapBuffers(dpy, surface);
}
