#pragma once
#include <android/log.h>
#include <cstdio>
#include <unistd.h>

#define LOG_TAG "zxMenu"
#define ENABLE_LOGGING 1

#if ENABLE_LOGGING
    #define LOGI(fmt, ...) do { \
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##__VA_ARGS__); \
    } while(0)

    #define LOGW(fmt, ...) do { \
        __android_log_print(ANDROID_LOG_WARN, LOG_TAG, fmt, ##__VA_ARGS__); \
    } while(0)

    #define LOGE(fmt, ...) do { \
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##__VA_ARGS__); \
    } while(0)

    #define LOGD(fmt, ...) do { \
        __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, fmt, ##__VA_ARGS__); \
    } while(0)
#else
    #define LOGI(...) ((void)0)
    #define LOGW(...) ((void)0)
    #define LOGE(...) ((void)0)
    #define LOGD(...) ((void)0)
#endif