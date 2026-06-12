#include <jni.h>
#include <pthread.h>
#include <dlfcn.h>
#include <atomic>
#include "dobby.h"
#include "Draw.h"
#include "Logger.h"

static std::atomic<bool> g_started{false};

static void* hook_thread(void*) {
    LOGI("hook_thread started, sleeping 5s");
    sleep(5);

    void* egl = dlopen("libEGL.so", RTLD_LAZY);
    if (!egl) {
        LOGE("dlopen libEGL.so failed");
        return nullptr;
    }
    LOGI("libEGL.so loaded at %p", egl);

    void* addr = dlsym(egl, "eglSwapBuffers");
    if (!addr) {
        LOGE("dlsym eglSwapBuffers failed");
        return nullptr;
    }
    LOGI("eglSwapBuffers found at %p", addr);

    int res = DobbyHook(addr, (void*)hooked_eglSwapBuffers, (void**)&orig_eglSwapBuffers);
    if (res == 0) {
        LOGI("DobbyHook success");
    } else {
        LOGE("DobbyHook failed, code %d", res);
    }

    return nullptr;
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* key) {
    LOGI("JNI_OnLoad called, key=%p", key);

    if (key != reinterpret_cast<void*>(1337)) {
        LOGI("key mismatch, bailing");
        return JNI_VERSION_1_6;
    }

    if (!g_started.exchange(true)) {
        LOGI("spawning hook thread");
        pthread_t t;
        pthread_create(&t, nullptr, hook_thread, nullptr);
        pthread_detach(t);
    }

    return JNI_VERSION_1_6;
}
