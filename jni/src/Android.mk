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
                    $(LOCAL_PATH)/Natives \
                    $(LOCAL_PATH)/Natives/ELF \
                    $(LOCAL_PATH)/App \
                    $(LOCAL_PATH)/App/Utils \
                    $(LOCAL_PATH)/Dobby

LOCAL_SRC_FILES := \
    App/Main.cpp \
    Natives/TouchHelperA.cpp \
    Graphics/GraphicsManager.cpp \
    Graphics/OpenGLGraphics.cpp \
    Graphics/VulkanGraphics.cpp \
    Graphics/SoftwareGraphics.cpp \
    Graphics/vulkan_wrapper.cpp \
    Natives/AndroidImgui.cpp \
    Natives/my_imgui.cpp \
    Natives/my_imgui_impl_android.cpp \
    Natives/ELF/elf_util.cpp

LOCAL_STATIC_LIBRARIES := Dobby
LOCAL_LDLIBS := -lm -ldl -lz -llog -landroid -lEGL -lGLESv1_CM -lGLESv2

include $(BUILD_SHARED_LIBRARY)
