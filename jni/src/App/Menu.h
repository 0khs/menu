#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include <math.h>
#include "CPU.h"

static void RainbowSeparator(float thickness, int alpha) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return;
    float x1 = window->DC.CursorPos.x;
    float x2 = window->WorkRect.Max.x;
    float y = window->DC.CursorPos.y + thickness * 0.5f;
    ImGui::ItemSize(ImVec2(0.0f, thickness));
    if (!ImGui::ItemAdd(ImRect(x1, y - thickness * 0.5f, x2, y + thickness * 0.5f), 0)) return;
    static float time = 0.0f;
    time += ImGui::GetIO().DeltaTime;
    float r = sinf(time * 2.0f) * 0.5f + 0.5f;
    float g = sinf(time * 2.0f + 2.094f) * 0.5f + 0.5f;
    float b = sinf(time * 2.0f + 4.188f) * 0.5f + 0.5f;
    ImU32 col = IM_COL32((int)(r*255), (int)(g*255), (int)(b*255), alpha);
    window->DrawList->AddRectFilled(ImVec2(x1, y - thickness * 0.5f), ImVec2(x2, y + thickness * 0.5f), col);
}

inline void DrawMenu() {
    static bool show_menu = true;
    if (!show_menu) return;
    ImGui::SetNextWindowSize(ImVec2(580, 380), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Mod Menu", &show_menu, ImGuiWindowFlags_NoCollapse)) {
        RainbowSeparator(3.0f, 255);
        if (ImGui::BeginTabBar("Tabs")) {
            if (ImGui::BeginTabItem("Features")) {
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Settings")) {
                ImGui::Text("FPS: %.1f | MS: %.2f", CPU::fps_ema, CPU::cur_ms);
                CPU::Graph();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}
