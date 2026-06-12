#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include <atomic>
#include <cstdarg>
#include <ctime>
#include <math.h>
#include "CPU.h"

#ifndef LAST_IMRECT_DEFINED
#define LAST_IMRECT_DEFINED
struct Last_ImRect { float Pos_x, Pos_y, Size_x, Size_y; };
#endif

static Last_ImRect LastCoordinate{0.f, 0.f, 0.f, 0.f};

static std::atomic<bool> g_menu_visible{true};

static void RainbowSeparator(float thickness, int alpha) {
    ImGuiWindow* w = ImGui::GetCurrentWindow();
    if (w->SkipItems) return;
    ImGuiContext& g = *GImGui;
    float x1    = w->DC.CursorPos.x;
    float x2    = w->WorkRect.Max.x;
    float cy    = w->DC.CursorPos.y + thickness * 0.5f;
    ImGui::ItemSize(ImVec2(0.f, thickness + g.Style.ItemSpacing.y));
    float width = x2 - x1;
    if (width <= 0.f) return;
    float t      = (float)ImGui::GetTime() * 3.0f;
    float offset = fmodf(t * 0.1f, 1.0f);
    const int SEG = 128;
    for (int i = 0; i < SEG; ++i) {
        float t0 = (float)i       / SEG;
        float t1 = (float)(i + 1) / SEG;
        float h0 = fmodf(t0 - offset + 1.f, 1.f);
        float h1 = fmodf(t1 - offset + 1.f, 1.f);
        ImVec4 c0{}, c1{};
        ImGui::ColorConvertHSVtoRGB(h0, 1.f, 1.f, c0.x, c0.y, c0.z);
        ImGui::ColorConvertHSVtoRGB(h1, 1.f, 1.f, c1.x, c1.y, c1.z);
        c0.w = c1.w = alpha / 255.f;
        float sx = x1 + t0 * width;
        float ex = x1 + t1 * width;
        w->DrawList->AddRectFilledMultiColor(
            {sx, cy - thickness * 0.5f}, {ex, cy + thickness * 0.5f},
            ImGui::ColorConvertFloat4ToU32(c0),
            ImGui::ColorConvertFloat4ToU32(c1),
            ImGui::ColorConvertFloat4ToU32(c1),
            ImGui::ColorConvertFloat4ToU32(c0));
    }
}

static void BoldTextColored(const ImVec4& col, const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    ImGui::TextColored(col, "%s", buf);
    ImVec2 min = ImGui::GetItemRectMin();
    ImGui::GetWindowDrawList()->AddText(
        {min.x + 1.f, min.y},
        ImGui::ColorConvertFloat4ToU32(col), buf);
}

inline void InitMenuStyle() {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontDefault();

    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding    = 6.f;
    s.FrameRounding     = 4.f;
    s.GrabRounding      = 4.f;
    s.ScrollbarRounding = 4.f;

    s.Colors[ImGuiCol_TitleBg]            = ImColor(18,  18,  18,  255);
    s.Colors[ImGuiCol_TitleBgActive]      = ImColor(18,  18,  18,  255);
    s.Colors[ImGuiCol_TitleBgCollapsed]   = ImColor(18,  18,  18,  255);
    s.Colors[ImGuiCol_WindowBg]           = ImColor(15,  15,  15,  245);
    s.Colors[ImGuiCol_FrameBg]            = ImColor(38,  38,  38,  255);
    s.Colors[ImGuiCol_FrameBgHovered]     = ImColor(50,  50,  50,  255);
    s.Colors[ImGuiCol_FrameBgActive]      = ImColor(42,  42,  42,  255);
    s.Colors[ImGuiCol_Button]             = ImColor(38,  38,  38,  255);
    s.Colors[ImGuiCol_ButtonHovered]      = ImColor(55,  55,  55,  255);
    s.Colors[ImGuiCol_ButtonActive]       = ImColor(42,  42,  42,  255);
    s.Colors[ImGuiCol_Header]             = ImColor(38,  38,  38,  255);
    s.Colors[ImGuiCol_HeaderHovered]      = ImColor(50,  50,  50,  255);
    s.Colors[ImGuiCol_HeaderActive]       = ImColor(42,  42,  42,  255);
    s.Colors[ImGuiCol_Tab]                = ImColor(28,  28,  28,  255);
    s.Colors[ImGuiCol_TabHovered]         = ImColor(55,  55,  55,  255);
    s.Colors[ImGuiCol_TabActive]          = ImColor(42,  42,  42,  255);
    s.Colors[ImGuiCol_TabUnfocused]       = ImColor(28,  28,  28,  255);
    s.Colors[ImGuiCol_TabUnfocusedActive] = ImColor(38,  38,  38,  255);
    s.Colors[ImGuiCol_CheckMark]          = ImColor(147, 255, 149, 255);
    s.Colors[ImGuiCol_SliderGrab]         = ImColor(147, 255, 149, 255);
    s.Colors[ImGuiCol_SliderGrabActive]   = ImColor(120, 220, 122, 255);
    s.Colors[ImGuiCol_Separator]          = ImColor(55,  55,  55,  255);
    s.Colors[ImGuiCol_SeparatorHovered]   = ImColor(55,  55,  55,  255);
    s.Colors[ImGuiCol_SeparatorActive]    = ImColor(55,  55,  55,  255);

    s.ScaleAllSizes(3.f);
}

inline void DrawMenu() {
    if (!g_menu_visible.load(std::memory_order_relaxed)) {
        LastCoordinate = {0.f, 0.f, 0.f, 0.f};
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(560.f, 0.f), ImGuiCond_Once);
    ImGui::Begin("OVERLAY", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    ImGuiWindow* win = ImGui::GetCurrentWindow();
    if (win) {
        constexpr float PAD = 40.f;
        LastCoordinate = {
            win->Pos.x  - PAD,
            win->Pos.y  - PAD,
            win->Size.x + PAD * 2.f,
            win->Size.y + PAD * 2.f
        };

        const char* ver  = "v1.0.0@alpha";
        ImVec2      tsz  = ImGui::CalcTextSize(ver);
        ImGui::GetForegroundDrawList()->AddText(
            {win->Pos.x + win->Size.x - tsz.x - 50.f,
             win->Pos.y + (win->TitleBarHeight - tsz.y) * 0.5f},
            IM_COL32(220, 60, 60, 255), ver);
    }

    BoldTextColored({0.58f, 1.f,  0.58f, 1.f}, "%.0f FPS",       CPU::fps_ema);
    ImGui::SameLine(300.f);
    BoldTextColored({0.70f, 0.70f, 0.70f, 1.f}, "%.2f ms/frame", CPU::cur_ms);
    CPU::Graph();
    RainbowSeparator(3.f, 255);

    if (ImGui::BeginTabBar("##tabs")) {
        if (ImGui::BeginTabItem("Features")) {
            ImGui::Spacing();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Info")) {
            ImGui::Spacing();

            char     date_buf[64];
            std::time_t now = std::time(nullptr);
            std::tm* tm  = std::localtime(&now);
            std::strftime(date_buf, sizeof(date_buf), "%Y-%m-%d  %H:%M:%S", tm);
            ImGui::TextColored({0.70f, 0.70f, 0.70f, 1.f}, "Date:");
            ImGui::SameLine();
            BoldTextColored({0.85f, 0.85f, 0.85f, 1.f}, "%s", date_buf);

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}
