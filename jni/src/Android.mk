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

LOCAL_C_INCLUDES += $(LOCAL_PATH)/ImGui \
                    $(LOCAL_PATH)/ImGui/Jenv \
                    $(LOCAL_PATH)/ImGui/ELF \
                    $(LOCAL_PATH)/App \
                    $(LOCAL_PATH)/App/Utils \
                    $(LOCAL_PATH)/Dobby

LOCAL_SRC_FILES := \
    App/Main.cpp \
    ImGui/TouchHelperA.cpp \
    ImGui/GraphicsManager.cpp \
    ImGui/OpenGLGraphics.cpp \
    ImGui/VulkanGraphics.cpp \
    ImGui/vulkan_wrapper.cpp \
    ImGui/AndroidImgui.cpp \
    ImGui/my_imgui.cpp \
    ImGui/my_imgui_impl_android.cpp \
    ImGui/Jenv/JavaFunc.cpp \
    ImGui/ELF/elf_util.cpp

LOCAL_STATIC_LIBRARIES := Dobby
LOCAL_LDLIBS := -lm -ldl -lz -llog -landroid -lEGL -lGLESv1_CM -lGLESv2 -lGLESv3 -lvulkan

include $(BUILD_SHARED_LIBRARY)
