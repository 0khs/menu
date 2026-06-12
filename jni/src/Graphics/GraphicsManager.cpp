#include "GraphicsManager.h"
#include "VulkanGraphics.h"
#include "OpenGLGraphics.h"
#include "Logger.h"

#include <cstdio>
#include <cstring>
#include <pthread.h>
#include <unistd.h>

namespace GraphicsManager {

static bool ProcessHasLib(const char* libName) {
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return false;

    char line[512];
    bool found = false;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, libName)) {
            found = true;
            break;
        }
    }
    fclose(f);
    return found;
}

static void* fallback_thread(void*) {
    sleep(3);
    if (!VulkanGraphics::IsActive()) {
        LOGI("GraphicsManager: Vulkan inactive after timeout, falling back to OpenGL");
        OpenGLGraphics::TryHook();
    }
    return nullptr;
}

void Init() {
    bool hasVulkan = ProcessHasLib("libvulkan.so");
    LOGI("GraphicsManager: libvulkan.so present in process: %d", (int)hasVulkan);

    if (hasVulkan) {
        if (!VulkanGraphics::TryHook()) {
            LOGI("GraphicsManager: Vulkan hook failed, using OpenGL");
            OpenGLGraphics::TryHook();
            return;
        }

        pthread_t t;
        pthread_create(&t, nullptr, fallback_thread, nullptr);
        pthread_detach(t);
    } else {
        OpenGLGraphics::TryHook();
    }
}

}
