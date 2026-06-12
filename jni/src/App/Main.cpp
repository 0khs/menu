#include <jni.h>
#include <pthread.h>
#include <unistd.h>
#include <dlfcn.h>
#include <android/log.h>

#include "dobby.h"

#include "Draw.h"
#include "Logger.h"

#define LOG_TAG "Menu"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

void* Main_thread(void*) {
    sleep(5);
    
    void* egl_handle = dlopen("libEGL.so", RTLD_LAZY);
    if (!egl_handle) {
        LOGI("libEGL.so not found");
        return nullptr;
    }

    void* eglSwapBuffers_addr = dlsym(egl_handle, "eglSwapBuffers");
    if (!eglSwapBuffers_addr) {
        LOGI("eglSwapBuffers not found");
        return nullptr;
    }

    if (DobbyHook(eglSwapBuffers_addr, (void*)hooked_eglSwapBuffers, (void**)&orig_eglSwapBuffers) == 0) {
        LOGI("DobbyHook success");
    } else {
        LOGI("DobbyHook failed");
    }

    return nullptr;
}

__attribute__((constructor))
void lib_main() {
    pthread_t t;
    pthread_create(&t, nullptr, Main_thread, nullptr);
}
