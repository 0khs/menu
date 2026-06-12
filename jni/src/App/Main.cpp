#include <jni.h>
#include <pthread.h>
#include <unistd.h>
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

    auto display = android::ANativeWindowCreator::GetDisplayInfo();
    if (display.height > display.width) {
        std::swap(display.height, display.width);
    }

    auto window = android::ANativeWindowCreator::Create("zxMenu", display.width, display.height);
    if (!window) {
        LOGE("Failed to create native window");
        g_started.store(false);
        return nullptr;
    }

    auto graphics = GraphicsManager::getGraphicsInterface(GraphicsManager::VULKAN);
    if (!graphics) {
        LOGE("Failed to create graphics interface");
        android::ANativeWindowCreator::Destroy(window);
        g_started.store(false);
        return nullptr;
    }

    if (!graphics->Init(window, display.width, display.height)) {
        LOGE("Graphics init failed, falling back to OpenGL");
        graphics = GraphicsManager::getGraphicsInterface(GraphicsManager::OPENGL);
        if (!graphics || !graphics->Init(window, display.width, display.height)) {
            LOGE("OpenGL fallback also failed");
            android::ANativeWindowCreator::Destroy(window);
            g_started.store(false);
            return nullptr;
        }
    }

    ImGui::Android_LoadSystemFont(26);
    Touch::Init({(float)display.width, (float)display.height}, true);

    g_running.store(true);

    while (g_running.load()) {
        graphics->NewFrame();
        Touch::setOrientation(android::ANativeWindowCreator::GetDisplayInfo().orientation);
        
        DrawFrame();
       

        graphics->EndFrame();
    }

    graphics->Shutdown();
    android::ANativeWindowCreator::Destroy(window);
    g_started.store(false);
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
