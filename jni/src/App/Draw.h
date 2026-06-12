#pragma once

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>

#include "imgui.h"
#include "imgui_impl_android.h"
#include "imgui_impl_opengl3.h"

#include "TouchHelperA.h"
#include "CPU.h"
#include "Menu.h"

static bool initImGui = false;
static int glWidth = 0, glHeight = 0;

static EGLBoolean (*orig_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);

static EGLBoolean hooked_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    eglQuerySurface(dpy, surface, EGL_WIDTH, &glWidth);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &glHeight);

    if (!initImGui) {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2((float)glWidth, (float)glHeight);

        ImGui_ImplAndroid_Init(nullptr);
        ImGui_ImplOpenGL3_Init("#version 300 es");
        
        Touch::Init({(float)glWidth, (float)glHeight}, false);
        initImGui = true;
    }

    CPU::Tick();

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)glWidth, (float)glHeight);
    io.DeltaTime = 1.0f / 60.0f;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();

    DrawMenu();

    Touch::SetMenuBounds(
        LastCoordinate.Pos_x, 
        LastCoordinate.Pos_y, 
        LastCoordinate.Size_x, 
        LastCoordinate.Size_y
    );

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    return orig_eglSwapBuffers(dpy, surface);
}
