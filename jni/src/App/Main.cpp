#include <jni.h>
#include <pthread.h>
#include <unistd.h>
#include <atomic>
#include "GraphicsManager.h"
#include "Logger.h"

static std::atomic<bool> g_started{false};

static void* hook_thread(void*) {
    LOGI("hook_thread started, sleeping 5s");
    sleep(5);

    GraphicsManager::Init();

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
