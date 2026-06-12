#include "VulkanGraphics.h"
#include "vulkan_wrapper.h"
#include "dobby.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "Menu.h"
#include "Logger.h"
#include "CPU.h"
#include "TouchHelperA.h"

#include <vector>
#include <cstring>

namespace VulkanGraphics {

static VkInstance s_instance = VK_NULL_HANDLE;
static VkPhysicalDevice s_physicalDevice = VK_NULL_HANDLE;
static VkDevice s_device = VK_NULL_HANDLE;
static VkQueue s_queue = VK_NULL_HANDLE;
static uint32_t s_queueFamily = 0;

static PFN_vkGetDeviceQueue vkGetDeviceQueue_ = nullptr;

static VkRenderPass s_renderPass = VK_NULL_HANDLE;
static VkDescriptorPool s_descriptorPool = VK_NULL_HANDLE;
static VkCommandPool s_cmdPool = VK_NULL_HANDLE;

static std::vector<VkImage> s_images;
static std::vector<VkImageView> s_imageViews;
static std::vector<VkFramebuffer> s_framebuffers;
static std::vector<VkCommandBuffer> s_cmdBuffers;
static std::vector<VkFence> s_fences;

static VkFormat s_format = VK_FORMAT_UNDEFINED;
static VkExtent2D s_extent{0, 0};

static bool s_imguiInit = false;
static bool s_active = false;

static decltype(&vkCreateInstance) orig_vkCreateInstance = nullptr;
static decltype(&vkCreateDevice) orig_vkCreateDevice = nullptr;
static decltype(&vkCreateSwapchainKHR) orig_vkCreateSwapchainKHR = nullptr;
static decltype(&vkQueuePresentKHR) orig_vkQueuePresentKHR = nullptr;

static void CheckVk(VkResult r) {
    if (r != VK_SUCCESS) LOGE("VulkanGraphics: vk error %d", (int)r);
}

static void DestroySwapchainResources() {
    if (s_device == VK_NULL_HANDLE) return;

    if (!s_fences.empty()) VkWrap::vkWaitForFences_(s_device, (uint32_t)s_fences.size(), s_fences.data(), VK_TRUE, UINT64_MAX);

    for (auto fb : s_framebuffers) VkWrap::vkDestroyFramebuffer_(s_device, fb, nullptr);
    for (auto iv : s_imageViews) VkWrap::vkDestroyImageView_(s_device, iv, nullptr);
    for (auto f : s_fences) VkWrap::vkDestroyFence_(s_device, f, nullptr);

    s_framebuffers.clear();
    s_imageViews.clear();
    s_fences.clear();
    s_cmdBuffers.clear();
    s_images.clear();

    if (s_renderPass) {
        VkWrap::vkDestroyRenderPass_(s_device, s_renderPass, nullptr);
        s_renderPass = VK_NULL_HANDLE;
    }
}

static void CreateSwapchainResources(VkSwapchainKHR swapchain, const VkSwapchainCreateInfoKHR* info) {
    DestroySwapchainResources();

    s_format = info->imageFormat;
    s_extent = info->imageExtent;

    uint32_t count = 0;
    VkWrap::vkGetSwapchainImagesKHR_(s_device, swapchain, &count, nullptr);
    s_images.resize(count);
    VkWrap::vkGetSwapchainImagesKHR_(s_device, swapchain, &count, s_images.data());

    VkAttachmentDescription attachment{};
    attachment.format = s_format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &attachment;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dep;
    CheckVk(VkWrap::vkCreateRenderPass_(s_device, &rpInfo, nullptr, &s_renderPass));

    s_imageViews.resize(count);
    s_framebuffers.resize(count);
    s_fences.resize(count);

    for (uint32_t i = 0; i < count; ++i) {
        VkImageViewCreateInfo ivInfo{};
        ivInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivInfo.image = s_images[i];
        ivInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivInfo.format = s_format;
        ivInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        ivInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        CheckVk(VkWrap::vkCreateImageView_(s_device, &ivInfo, nullptr, &s_imageViews[i]));

        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = s_renderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &s_imageViews[i];
        fbInfo.width = s_extent.width;
        fbInfo.height = s_extent.height;
        fbInfo.layers = 1;
        CheckVk(VkWrap::vkCreateFramebuffer_(s_device, &fbInfo, nullptr, &s_framebuffers[i]));

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        CheckVk(VkWrap::vkCreateFence_(s_device, &fenceInfo, nullptr, &s_fences[i]));
    }

    if (s_cmdPool == VK_NULL_HANDLE) {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = s_queueFamily;
        CheckVk(VkWrap::vkCreateCommandPool_(s_device, &poolInfo, nullptr, &s_cmdPool));
    }

    s_cmdBuffers.resize(count);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = s_cmdPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = count;
    CheckVk(VkWrap::vkAllocateCommandBuffers_(s_device, &allocInfo, s_cmdBuffers.data()));

    if (!s_imguiInit) {
        VkDescriptorPoolSize poolSizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 64 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64 },
        };
        VkDescriptorPoolCreateInfo dpInfo{};
        dpInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        dpInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        dpInfo.maxSets = 64;
        dpInfo.poolSizeCount = 2;
        dpInfo.pPoolSizes = poolSizes;
        CheckVk(VkWrap::vkCreateDescriptorPool_(s_device, &dpInfo, nullptr, &s_descriptorPool));

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2((float)s_extent.width, (float)s_extent.height);

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.Instance = s_instance;
        initInfo.PhysicalDevice = s_physicalDevice;
        initInfo.Device = s_device;
        initInfo.QueueFamily = s_queueFamily;
        initInfo.Queue = s_queue;
        initInfo.DescriptorPool = s_descriptorPool;
        initInfo.RenderPass = s_renderPass;
        initInfo.Subpass = 0;
        initInfo.MinImageCount = count;
        initInfo.ImageCount = count;
        initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        initInfo.CheckVkResultFn = CheckVk;
        ImGui_ImplVulkan_Init(&initInfo);

        VkWrap::vkResetCommandPool_(s_device, s_cmdPool, 0);
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VkWrap::vkBeginCommandBuffer_(s_cmdBuffers[0], &beginInfo);
        ImGui_ImplVulkan_CreateFontsTexture();
        VkWrap::vkEndCommandBuffer_(s_cmdBuffers[0]);

        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &s_cmdBuffers[0];
        VkWrap::vkQueueSubmit_(s_queue, 1, &submit, VK_NULL_HANDLE);
        VkWrap::vkDeviceWaitIdle_(s_device);

        InitMenuStyle();
        Touch::Init({(float)s_extent.width, (float)s_extent.height}, false);

        s_imguiInit = true;
        LOGI("VulkanGraphics: ImGui init done %ux%u", s_extent.width, s_extent.height);
    } else {
        ImGui_ImplVulkan_SetMinImageCount(count);
    }
}

static VkResult hooked_vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* info, const VkAllocationCallbacks* alloc, VkSwapchainKHR* swapchain) {
    VkResult res = orig_vkCreateSwapchainKHR(device, info, alloc, swapchain);
    if (res == VK_SUCCESS) {
        LOGI("VulkanGraphics: swapchain created %ux%u fmt=%d", info->imageExtent.width, info->imageExtent.height, (int)info->imageFormat);
        CreateSwapchainResources(*swapchain, info);
    }
    return res;
}

static VkResult hooked_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* presentInfo) {
    if (s_imguiInit && presentInfo->swapchainCount > 0 && !s_framebuffers.empty()) {
        uint32_t imageIndex = presentInfo->pImageIndices[0];

        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2((float)s_extent.width, (float)s_extent.height);
        io.DeltaTime = 1.0f / 60.0f;

        CPU::Tick();

        ImGui_ImplVulkan_NewFrame();
        ImGui::NewFrame();

        DrawMenu();

        Touch::SetMenuBounds(LastCoordinate.Pos_x, LastCoordinate.Pos_y, LastCoordinate.Size_x, LastCoordinate.Size_y);

        ImGui::EndFrame();
        ImGui::Render();
        ImDrawData* drawData = ImGui::GetDrawData();

        VkWrap::vkWaitForFences_(s_device, 1, &s_fences[imageIndex], VK_TRUE, UINT64_MAX);
        VkWrap::vkResetFences_(s_device, 1, &s_fences[imageIndex]);

        VkCommandBuffer cmd = s_cmdBuffers[imageIndex];
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VkWrap::vkBeginCommandBuffer_(cmd, &beginInfo);

        VkRenderPassBeginInfo rpBegin{};
        rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBegin.renderPass = s_renderPass;
        rpBegin.framebuffer = s_framebuffers[imageIndex];
        rpBegin.renderArea.extent = s_extent;
        VkWrap::vkCmdBeginRenderPass_(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

        ImGui_ImplVulkan_RenderDrawData(drawData, cmd);

        VkWrap::vkCmdEndRenderPass_(cmd);
        VkWrap::vkEndCommandBuffer_(cmd);

        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        VkWrap::vkQueueSubmit_(s_queue, 1, &submit, s_fences[imageIndex]);
        VkWrap::vkWaitForFences_(s_device, 1, &s_fences[imageIndex], VK_TRUE, UINT64_MAX);
    }

    return orig_vkQueuePresentKHR(queue, presentInfo);
}

static VkResult hooked_vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* info, const VkAllocationCallbacks* alloc, VkDevice* device) {
    VkResult res = orig_vkCreateDevice(physicalDevice, info, alloc, device);
    if (res != VK_SUCCESS) return res;

    s_physicalDevice = physicalDevice;
    s_device = *device;

    if (info->queueCreateInfoCount > 0) {
        s_queueFamily = info->pQueueCreateInfos[0].queueFamilyIndex;
    }

    VkWrap::ResolveDeviceFunctions(s_device);

    vkGetDeviceQueue_ = (PFN_vkGetDeviceQueue)VkWrap::vkGetDeviceProcAddr_(s_device, "vkGetDeviceQueue");
    vkGetDeviceQueue_(s_device, s_queueFamily, 0, &s_queue);

    LOGI("VulkanGraphics: device created, queueFamily=%u", s_queueFamily);

    if (VkWrap::vkCreateSwapchainKHR_ && !orig_vkCreateSwapchainKHR) {
        DobbyHook((void*)VkWrap::vkCreateSwapchainKHR_, (void*)hooked_vkCreateSwapchainKHR, (void**)&orig_vkCreateSwapchainKHR);
    }
    if (VkWrap::vkQueuePresentKHR_ && !orig_vkQueuePresentKHR) {
        DobbyHook((void*)VkWrap::vkQueuePresentKHR_, (void*)hooked_vkQueuePresentKHR, (void**)&orig_vkQueuePresentKHR);
    }

    return res;
}

static VkResult hooked_vkCreateInstance(const VkInstanceCreateInfo* info, const VkAllocationCallbacks* alloc, VkInstance* instance) {
    VkResult res = orig_vkCreateInstance(info, alloc, instance);
    if (res != VK_SUCCESS) return res;

    s_instance = *instance;
    VkWrap::ResolveInstanceFunctions(s_instance);

    LOGI("VulkanGraphics: instance created %p", (void*)s_instance);

    if (VkWrap::vkCreateDevice_ && !orig_vkCreateDevice) {
        DobbyHook((void*)VkWrap::vkCreateDevice_, (void*)hooked_vkCreateDevice, (void**)&orig_vkCreateDevice);
    }

    s_active = true;
    return res;
}

bool TryHook() {
    if (!VkWrap::Load()) return false;

    orig_vkCreateInstance = VkWrap::vkCreateInstance_;
    int res = DobbyHook((void*)VkWrap::vkCreateInstance_, (void*)hooked_vkCreateInstance, (void**)&orig_vkCreateInstance);
    if (res != 0) {
        LOGE("VulkanGraphics: failed to hook vkCreateInstance, code %d", res);
        return false;
    }

    LOGI("VulkanGraphics: vkCreateInstance hooked");
    return true;
}

bool IsActive() {
    return s_active;
}

}
