#include <jni.h>
#include <pthread.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <atomic>
#include "ANativeWindowCreator.h"
#include "GraphicsManager.h"
#include "my_imgui.h"
#include "TouchHelperA.h"
#include "Logger.h"
#include "Draw.h"

static std::atomic<bool> g_started{false};
static std::atomic<bool> g_running{false};

static void* hook_thread(void*) {
    LOGI("hook_thread started, sleeping 5s");
    sleep(5);

    if (g_running.load()) {
        LOGW("Another thread is already running, exiting");
        g_started.store(false);
        return nullptr;
    }
    g_running.store(true);

    while (g_running.load()) {
        android::ANativeWindowCreator::DisplayInfo display{};
        for (int i = 0; i < 20; i++) {
            display = android::ANativeWindowCreator::GetDisplayInfo();
            LOGI("Display attempt %d: %dx%d", i, display.width, display.height);
            if (display.width > 0 && display.height > 0) break;
            sleep(1);
        }

        auto window = android::ANativeWindowCreator::Create("zxMenu", display.width, display.height, false);
        LOGI("Window: %p", window);
        if (!window) {
            LOGE("Failed to create native window");
            sleep(1);
            continue;
        }

        if (display.width <= 0 || display.height <= 0) {
            display.width  = ANativeWindow_getWidth(window);
            display.height = ANativeWindow_getHeight(window);
            LOGI("Display from window: %dx%d", display.width, display.height);
        }

        if (display.width <= 0 || display.height <= 0) {
            LOGE("Failed to get display dimensions");
            android::ANativeWindowCreator::Destroy(window);
            sleep(1);
            continue;
        }

        if (display.height > display.width)
            std::swap(display.height, display.width);

        std::unique_ptr<AndroidImgui> graphics = nullptr;
        GraphicsManager::GraphicsAPI selectedApi = GraphicsManager::VULKAN;
        bool ok = false;

        graphics = GraphicsManager::getGraphicsInterface(GraphicsManager::VULKAN);
        LOGI("Vulkan graphics ptr: %p", graphics.get());
        if (graphics) {
            ok = graphics->Init(window, display.width, display.height);
            LOGI("Vulkan init result: %d", ok);
        }

        if (!ok) {
            LOGE("Vulkan failed, trying OpenGL");
            graphics = GraphicsManager::getGraphicsInterface(GraphicsManager::OPENGL);
            LOGI("OpenGL graphics ptr: %p", graphics.get());
            if (graphics) {
                ok = graphics->Init(window, display.width, display.height);
                LOGI("OpenGL init result: %d", ok);
            }
        }

        if (!ok) {
            LOGE("OpenGL failed, trying Software");
            graphics = GraphicsManager::getGraphicsInterface(GraphicsManager::SOFTWARE);
            LOGI("Software graphics ptr: %p", graphics.get());
            if (graphics) {
                ok = graphics->Init(window, display.width, display.height);
                LOGI("Software init result: %d", ok);
            }
        }

        if (!ok || !graphics) {
            LOGE("All graphics backends failed");
            android::ANativeWindowCreator::Destroy(window);
            sleep(2);
            continue;
        }

        LOGI("Graphics backend initialized successfully");

        ImGui::Android_LoadSystemFont(26);
        InitMenuStyle();
        Touch::Init({(float)display.width, (float)display.height}, true);

        while (g_running.load()) {
            graphics->NewFrame();
            Touch::setOrientation(android::ANativeWindowCreator::GetDisplayInfo().orientation);
            DrawFrame();
            graphics->EndFrame();

            if (ANativeWindow_getWidth(window) <= 0) {
                LOGI("Window invalidated, restarting");
                break;
            }

            usleep(16000);
        }

        graphics->Shutdown();
        android::ANativeWindowCreator::Destroy(window);
        sleep(1);
    }

    g_started.store(false);
    g_running.store(false);
    return nullptr;
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* key) {
    android::ANativeWindowCreator::SetVM(vm);
    LOGI("JNI_OnLoad called, key=%p", key);

    if (key != reinterpret_cast<void*>(1337)) {
        LOGI("key mismatch, bailing");
        return JNI_VERSION_1_6;
    }

    char cmdline[256] = {0};
    FILE* f = fopen("/proc/self/cmdline", "r");
    if (f) {
        fread(cmdline, 1, sizeof(cmdline) - 1, f);
        fclose(f);
    }
    if (strcmp(cmdline, "com.ForgeGames.SpecialForcesGroup2") != 0) {
        LOGI("non-main process (%s), bailing", cmdline);
        return JNI_VERSION_1_6;
    }


    if (!g_started.exchange(true)) {
        LOGI("==============================================================");
        LOGI("    ███████╗██╗  ██╗███╗   ███╗███████╗███╗   ██╗██╗   ██╗");
        LOGI("    ╚══███╔╝╚██╗██╔╝████╗ ████║██╔════╝████╗  ██║██║   ██║");
        LOGI("      ███╔╝  ╚███╔╝ ██╔████╔██║█████╗  ██╔██╗ ██║██║   ██║");
        LOGI("     ███╔╝   ██╔██╗ ██║╚██╔╝██║██╔══╝  ██║╚██╗██║██║   ██║");
        LOGI("    ███████╗██╔╝ ██╗██║ ╚═╝ ██║███████╗██║ ╚████║╚██████╔╝");
        LOGI("    ╚══════╝╚═╝  ╚═╝╚═╝     ╚═╝╚══════╝╚═╝  ╚═══╝ ╚═════╝ ");
        LOGI("==============================================================");
        LOGI("                zxMenu Initialized Successfully");
        LOGI("--------------------------------------------------------------");
        LOGI("                    Developed by @O0khs");
        LOGI("==============================================================");
        pthread_t t;
        pthread_create(&t, nullptr, hook_thread, nullptr);
        pthread_detach(t);
    } else {
        LOGI("zxMenu already started, skipping");
    }

    return JNI_VERSION_1_6;
}
