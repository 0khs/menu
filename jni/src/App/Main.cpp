#include <jni.h>
#include <pthread.h>
#include <unistd.h>
#include <android/native_window_jni.h>
#include <atomic>
#include "Jenv/JavaFunc.h"
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

    if (JavaFunc::init() < 0) {
        LOGI("JavaFunc init failed");
        return nullptr;
    }

    auto display = JavaFunc::getDisplayInfo();
    if (display.height > display.width) {
        std::swap(display.height, display.width);
    }

    jobject view = JavaFunc::getView(display.width, display.height, false, false);

    jobject surface = nullptr;
    for (int i = 0; i < 10; i++) {
        sleep(1);
        surface = JavaFunc::getSurface(view);
        if (surface) {
            break;
        }
    }

    if (!surface) {
        LOGI("Failed to get surface");
        return nullptr;
    }

    auto window = ANativeWindow_fromSurface(JavaFunc::GetJavaEnv(), surface);
    auto graphics = GraphicsManager::getGraphicsInterface(GraphicsManager::VULKAN);

    graphics->Init(window, display.width, display.height);
    ImGui::Android_LoadSystemFont(26);
    Touch::Init({(float)display.width, (float)display.height}, true);
    InitMenuStyle();

    g_running.store(true);

    while (g_running.load()) {
        graphics->NewFrame();
        Touch::setOrientation(JavaFunc::getDisplayInfo().orientation);

        DrawFrame();

        graphics->EndFrame();
    }

    graphics->Shutdown();
    JavaFunc::removeView(view);

    return nullptr;
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* key) {
    LOGI("JNI_OnLoad called, key=%p", key);

    if (key != reinterpret_cast<void*>(1337)) {
        LOGI("key mismatch, bailing");
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
        LOGI("    Developed by @O0khs");
        LOGI("==============================================================");
        pthread_t t;
        pthread_create(&t, nullptr, hook_thread, nullptr);
        pthread_detach(t);
    }

    return JNI_VERSION_1_6;
}
