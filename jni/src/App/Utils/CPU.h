#pragma once
#include <chrono>
#include "imgui.h"

namespace CPU {
    inline constexpr int HISTORY = 128;
    inline float samples[HISTORY] = {};
    inline int   head    = 0;
    inline float cur_ms  = 0.f;
    inline float fps_ema = 0.f;
    inline std::chrono::high_resolution_clock::time_point last_tick;

    inline void Tick() {
        auto now = std::chrono::high_resolution_clock::now();
        if (last_tick.time_since_epoch().count() != 0) {
            std::chrono::duration<float, std::milli> dur = now - last_tick;
            cur_ms = dur.count();

            samples[head] = cur_ms;
            head = (head + 1) % HISTORY;

            float sum   = 0.f;
            int   count = 0;
            for (float s : samples)
                if (s > 0.f) { sum += s; count++; }

            float avg_ms      = (count > 0) ? (sum / static_cast<float>(count)) : 16.66f;
            float instant_fps = (avg_ms  > 0.f) ? (1000.f / avg_ms) : 60.f;

            if (fps_ema == 0.f) fps_ema = instant_fps;
            else                fps_ema = (fps_ema * 0.95f) + (instant_fps * 0.05f);
        }
        last_tick = now;
    }

    inline void Graph() {
        ImGui::PlotLines("##frametimes", samples, HISTORY, head,
            nullptr, 0.f, 33.33f, ImVec2(-1, 50));
    }
}