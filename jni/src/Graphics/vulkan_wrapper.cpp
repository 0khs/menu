#include "vulkan_wrapper.h"
#include <dlfcn.h>
#include "Logger.h"

namespace VkWrap {

PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr_ = nullptr;
PFN_vkCreateInstance vkCreateInstance_ = nullptr;
PFN_vkCreateDevice vkCreateDevice_ = nullptr;
PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr_ = nullptr;
PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR_ = nullptr;
PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR_ = nullptr;
PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR_ = nullptr;
PFN_vkQueuePresentKHR vkQueuePresentKHR_ = nullptr;
PFN_vkCreateImageView vkCreateImageView_ = nullptr;
PFN_vkDestroyImageView vkDestroyImageView_ = nullptr;
PFN_vkCreateRenderPass vkCreateRenderPass_ = nullptr;
PFN_vkDestroyRenderPass vkDestroyRenderPass_ = nullptr;
PFN_vkCreateFramebuffer vkCreateFramebuffer_ = nullptr;
PFN_vkDestroyFramebuffer vkDestroyFramebuffer_ = nullptr;
PFN_vkCreateCommandPool vkCreateCommandPool_ = nullptr;
PFN_vkResetCommandPool vkResetCommandPool_ = nullptr;
PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers_ = nullptr;
PFN_vkBeginCommandBuffer vkBeginCommandBuffer_ = nullptr;
PFN_vkEndCommandBuffer vkEndCommandBuffer_ = nullptr;
PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass_ = nullptr;
PFN_vkCmdEndRenderPass vkCmdEndRenderPass_ = nullptr;
PFN_vkQueueSubmit vkQueueSubmit_ = nullptr;
PFN_vkCreateFence vkCreateFence_ = nullptr;
PFN_vkWaitForFences vkWaitForFences_ = nullptr;
PFN_vkResetFences vkResetFences_ = nullptr;
PFN_vkDestroyFence vkDestroyFence_ = nullptr;
PFN_vkCreateSemaphore vkCreateSemaphore_ = nullptr;
PFN_vkDestroySemaphore vkDestroySemaphore_ = nullptr;
PFN_vkCreateDescriptorPool vkCreateDescriptorPool_ = nullptr;
PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties_ = nullptr;
PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties_ = nullptr;
PFN_vkDeviceWaitIdle vkDeviceWaitIdle_ = nullptr;

static void* s_lib = nullptr;

bool Load() {
    if (s_lib) return true;

    s_lib = dlopen("libvulkan.so", RTLD_NOW);
    if (!s_lib) {
        LOGE("vulkan_wrapper: dlopen libvulkan.so failed");
        return false;
    }

    vkGetInstanceProcAddr_ = (PFN_vkGetInstanceProcAddr)dlsym(s_lib, "vkGetInstanceProcAddr");
    vkCreateInstance_      = (PFN_vkCreateInstance)dlsym(s_lib, "vkCreateInstance");

    if (!vkGetInstanceProcAddr_ || !vkCreateInstance_) {
        LOGE("vulkan_wrapper: core symbols missing");
        return false;
    }

    LOGI("vulkan_wrapper: loaded libvulkan.so at %p", s_lib);
    return true;
}

void ResolveInstanceFunctions(VkInstance instance) {
    auto get = vkGetInstanceProcAddr_;

    vkCreateDevice_      = (PFN_vkCreateDevice)get(instance, "vkCreateDevice");
    vkGetDeviceProcAddr_ = (PFN_vkGetDeviceProcAddr)get(instance, "vkGetDeviceProcAddr");

    vkGetPhysicalDeviceMemoryProperties_ =
        (PFN_vkGetPhysicalDeviceMemoryProperties)get(instance, "vkGetPhysicalDeviceMemoryProperties");
    vkGetPhysicalDeviceQueueFamilyProperties_ =
        (PFN_vkGetPhysicalDeviceQueueFamilyProperties)get(instance, "vkGetPhysicalDeviceQueueFamilyProperties");
}

void ResolveDeviceFunctions(VkDevice device) {
    auto get = vkGetDeviceProcAddr_;

    vkCreateSwapchainKHR_     = (PFN_vkCreateSwapchainKHR)get(device, "vkCreateSwapchainKHR");
    vkDestroySwapchainKHR_    = (PFN_vkDestroySwapchainKHR)get(device, "vkDestroySwapchainKHR");
    vkGetSwapchainImagesKHR_  = (PFN_vkGetSwapchainImagesKHR)get(device, "vkGetSwapchainImagesKHR");
    vkQueuePresentKHR_        = (PFN_vkQueuePresentKHR)get(device, "vkQueuePresentKHR");

    vkCreateImageView_   = (PFN_vkCreateImageView)get(device, "vkCreateImageView");
    vkDestroyImageView_  = (PFN_vkDestroyImageView)get(device, "vkDestroyImageView");
    vkCreateRenderPass_  = (PFN_vkCreateRenderPass)get(device, "vkCreateRenderPass");
    vkDestroyRenderPass_ = (PFN_vkDestroyRenderPass)get(device, "vkDestroyRenderPass");
    vkCreateFramebuffer_  = (PFN_vkCreateFramebuffer)get(device, "vkCreateFramebuffer");
    vkDestroyFramebuffer_ = (PFN_vkDestroyFramebuffer)get(device, "vkDestroyFramebuffer");

    vkCreateCommandPool_       = (PFN_vkCreateCommandPool)get(device, "vkCreateCommandPool");
    vkResetCommandPool_        = (PFN_vkResetCommandPool)get(device, "vkResetCommandPool");
    vkAllocateCommandBuffers_  = (PFN_vkAllocateCommandBuffers)get(device, "vkAllocateCommandBuffers");
    vkBeginCommandBuffer_      = (PFN_vkBeginCommandBuffer)get(device, "vkBeginCommandBuffer");
    vkEndCommandBuffer_        = (PFN_vkEndCommandBuffer)get(device, "vkEndCommandBuffer");
    vkCmdBeginRenderPass_      = (PFN_vkCmdBeginRenderPass)get(device, "vkCmdBeginRenderPass");
    vkCmdEndRenderPass_        = (PFN_vkCmdEndRenderPass)get(device, "vkCmdEndRenderPass");

    vkQueueSubmit_   = (PFN_vkQueueSubmit)get(device, "vkQueueSubmit");
    vkCreateFence_   = (PFN_vkCreateFence)get(device, "vkCreateFence");
    vkWaitForFences_ = (PFN_vkWaitForFences)get(device, "vkWaitForFences");
    vkResetFences_   = (PFN_vkResetFences)get(device, "vkResetFences");
    vkDestroyFence_  = (PFN_vkDestroyFence)get(device, "vkDestroyFence");

    vkCreateSemaphore_  = (PFN_vkCreateSemaphore)get(device, "vkCreateSemaphore");
    vkDestroySemaphore_ = (PFN_vkDestroySemaphore)get(device, "vkDestroySemaphore");

    vkCreateDescriptorPool_ = (PFN_vkCreateDescriptorPool)get(device, "vkCreateDescriptorPool");
    vkDeviceWaitIdle_       = (PFN_vkDeviceWaitIdle)get(device, "vkDeviceWaitIdle");
}

}
