LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := Dobby
LOCAL_SRC_FILES := Dobby/$(TARGET_ARCH_ABI)/libdobby.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := menu

LOCAL_CFLAGS := -w -s -fvisibility=hidden -fpermissive -fexceptions -O3
LOCAL_CPPFLAGS := -w -s -fvisibility=hidden -Werror -std=c++17 -Wno-error=c++11-narrowing -fpermissive -Wall -fexceptions -O3
LOCAL_LDFLAGS += -Wl,--version-script=$(LOCAL_PATH)/export.map


LOCAL_C_INCLUDES += $(LOCAL_PATH)/imGui \
                    $(LOCAL_PATH)/App/Touch \
                    $(LOCAL_PATH)/App/Utils \
                    $(LOCAL_PATH)/App/Graphics \
                    $(LOCAL_PATH)/App \
                    $(LOCAL_PATH)/Dobby

LOCAL_SRC_FILES := \
    App/Main.cpp \
    App/Touch/TouchHelperA.cpp \
    App/Graphics/GraphicsManager.cpp \
    App/Graphics/OpenGLGraphics.cpp \
    App/Graphics/VulkanGraphics.cpp \
    App/Graphics/vulkan_wrapper.cpp \
    imGui/imgui.cpp \
    imGui/imgui_draw.cpp \
    imGui/imgui_tables.cpp \
    imGui/imgui_widgets.cpp \
    imGui/imgui_impl_android.cpp \
    imGui/imgui_impl_opengl3.cpp \
    imGui/imgui_impl_vulkan.cpp

LOCAL_STATIC_LIBRARIES := Dobby
LOCAL_LDLIBS := -lm -ldl -lz -llog -landroid -lEGL -lGLESv1_CM -lGLESv2 -lGLESv3 -lvulkan

include $(BUILD_SHARED_LIBRARY)
$(call import-module,android/native_app_glue)
