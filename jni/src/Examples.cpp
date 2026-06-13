#include <jni.h>
#include <pthread.h>
#include <unistd.h>
#include <atomic>

// Include the updated Native Creator and Graphics hooks
#include "ANativeWindowCreator.h"
#include "GraphicsManager.h"
#include "my_imgui.h"
#include "TouchHelperA.h"

// Global state flags to manage our detached thread
static std::atomic<bool> g_is_injected{false};
static std::atomic<bool> g_is_running{false};

// The core rendering and logic loop runs here. We offload it so we don't block the host app's main thread.
static void* MainRenderThread(void*) {
    // Wait for the host app to fully load and register its displays. 
    // Trying to fetch the surface immediately often results in 0x0 or crashing.
    sleep(3);

    android::ANativeWindowCreator::DisplayInfo displayInfo{};
    
    // We loop to fetch the display. Our updated class will automatically fall back to JNI 
    // if SurfaceComposerClient fails due to Android 14+ strict layer restrictions.
    for (int attempts = 0; attempts < 15; attempts++) {
        displayInfo = android::ANativeWindowCreator::GetDisplayInfo();
        if (displayInfo.width > 0 && displayInfo.height > 0) {
            break;
        }
        sleep(1);
    }

    // Safety net: if after 15 seconds we still have nothing, bail out safely to prevent a memory leak or crash.
    if (displayInfo.width <= 0 || displayInfo.height <= 0) {
        g_is_injected.store(false);
        return nullptr;
    }

    // Always ensure the width is the smaller dimension (portrait mode bounds) 
    // so touches and UI scaling map properly regardless of initial device rotation.
    if (displayInfo.height > displayInfo.width) {
        std::swap(displayInfo.height, displayInfo.width);
    }

    // Request our native window surface overlay.
    // 'true' hides it from screen recorders/screenshots, 'false' ensures it's not enforcing SECURE_FLAG unnecessarily.
    auto nativeWindow = android::ANativeWindowCreator::Create("OverlayMenu", displayInfo.width, displayInfo.height, true, false);
    if (!nativeWindow) {
        g_is_injected.store(false);
        return nullptr;
    }

    // High performance priority: Always request Vulkan first. 
    // It reduces CPU overhead drastically compared to OpenGL.
    auto graphics = GraphicsManager::getGraphicsInterface(GraphicsManager::VULKAN);
    bool isGraphicsReady = false;

    if (graphics) {
        isGraphicsReady = graphics->Init(nativeWindow, displayInfo.width, displayInfo.height);
    }

    // Quality fallback: If the device doesn't support Vulkan or the initialization fails,
    // gracefully fall back to OpenGL.
    if (!isGraphicsReady) {
        graphics = GraphicsManager::getGraphicsInterface(GraphicsManager::OPENGL);
        if (graphics) {
            isGraphicsReady = graphics->Init(nativeWindow, displayInfo.width, displayInfo.height);
        }
    }

    // If both graphic interfaces fail, destroy the window and exit cleanly.
    if (!isGraphicsReady) {
        android::ANativeWindowCreator::Destroy(nativeWindow);
        g_is_injected.store(false);
        return nullptr;
    }

    // Initialize ImGui and inputs.
    // Load a slightly larger system font for readability on high PPI screens.
    ImGui::Android_LoadSystemFont(26);
    
    // Initialize touch mapping boundaries
    Touch::Init({(float)displayInfo.width, (float)displayInfo.height}, true);

    g_is_running.store(true);
    bool show_menu = true;

    // Main render pipeline
    while (g_is_running.load()) {
        graphics->NewFrame();
        
        // Dynamically update the orientation each frame so the touch matrix doesn't invert if the user rotates the phone
        Touch::setOrientation(android::ANativeWindowCreator::GetDisplayInfo().orientation);

        // ImGui Window Code
        ImGui::SetNextWindowSize({600, 400}, ImGuiCond_Once);
        if (ImGui::Begin("High Performance Overlay", &show_menu, ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Render Backend Active.");
            ImGui::Text("Resolution: %dx%d", displayInfo.width, displayInfo.height);
            ImGui::Text("Application Framerate: %.1f FPS", ImGui::GetIO().Framerate);
            
            ImGui::Separator();
            
            if (ImGui::Button("Close Overlay")) {
                g_is_running.store(false);
            }
        }
        ImGui::End();

        graphics->EndFrame();
    }

    // Deconstruct and cleanup memory when the loop breaks
    graphics->Shutdown();
    android::ANativeWindowCreator::Destroy(nativeWindow);
    g_is_injected.store(false);

    return nullptr;
}

// Entry point when the dynamic library is injected/loaded by the JVM or your frickin injector(Spesifically AndKittyInjector)
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    // 1. Pass the JavaVM pointer to the Native Window class so the internal fallback can use it immediately.
    android::ANativeWindowCreator::SetVM(vm);

    // 2. Prevent multiple threads from spawning if OnLoad triggers multiple times
    if (!g_is_injected.exchange(true)) {
        pthread_t overlayThread;
        pthread_create(&overlayThread, nullptr, MainRenderThread, nullptr);
        pthread_detach(overlayThread);
    }

    return JNI_VERSION_1_6;
}
