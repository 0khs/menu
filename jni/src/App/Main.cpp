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

    android::ANativeWindowCreator::DisplayInfo display{};
    for (int i = 0; i < 20; i++) {
        display = android::ANativeWindowCreator::GetDisplayInfo();
        LOGI("Display attempt %d: %dx%d", i, display.width, display.height);
        if (display.width > 0 && display.height > 0) break;
        sleep(1);
    }

    auto window = android::ANativeWindowCreator::Create("zxMenu", display.width, display.height);
    LOGI("Window: %p", window);
    if (!window) {
        LOGE("Failed to create native window");
        g_started.store(false);
        return nullptr;
    }

    if (display.width <= 0 || display.height <= 0) {
        display.width  = ANativeWindow_getWidth(window);
        display.height = ANativeWindow_getHeight(window);
        LOGI("Display from window: %dx%d", display.width, display.height);
    }

    if (display.width <= 0 || display.height <= 0) {
        LOGE("Failed to get display dimensions");
        android::ANativeWindowCreator::Destroy(window);
        g_started.store(false);
        return nullptr;
    }

    if (display.height > display.width)
        std::swap(display.height, display.width);

    auto graphics = GraphicsManager::getGraphicsInterface(GraphicsManager::VULKAN);
    LOGI("Graphics ptr: %p", graphics.get());
    if (!graphics) {
        LOGE("Failed to create graphics interface");
        android::ANativeWindowCreator::Destroy(window);
        g_started.store(false);
        return nullptr;
    }

    bool ok = graphics->Init(window, display.width, display.height);
    LOGI("Vulkan init result: %d", ok);
    if (!ok) {
        LOGE("Graphics init failed, falling back to OpenGL");
        graphics = GraphicsManager::getGraphicsInterface(GraphicsManager::OPENGL);
        ok = graphics && graphics->Init(window, display.width, display.height);
        LOGI("OpenGL fallback result: %d", ok);
        if (!ok) {
            LOGE("OpenGL fallback also failed");
            android::ANativeWindowCreator::Destroy(window);
            g_started.store(false);
            return nullptr;
        }
    }

    ImGui::Android_LoadSystemFont(26);
    InitMenuStyle();
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
    android::ANativeWindowCreator::SetVM(vm);
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
        LOGI("                    Developed by @O0khs");
        LOGI("==============================================================");
        pthread_t t;
        pthread_create(&t, nullptr, hook_thread, nullptr);
        pthread_detach(t);
    }

    return JNI_VERSION_1_6;
}
