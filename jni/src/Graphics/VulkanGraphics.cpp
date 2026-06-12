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

static VkResult hooked_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
    uint32_t index = pPresentInfo->pImageIndices[0];

    VkWrap::vkWaitForFences_(s_device, 1, &s_fences[index], VK_TRUE, UINT64_MAX);
    VkWrap::vkResetFences_(s_device, 1, &s_fences[index]);

    VkCommandBuffer cmd = s_cmdBuffers[index];

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VkWrap::vkBeginCommandBuffer_(cmd, &beginInfo);

    VkRenderPassBeginInfo rpBeginInfo{};
    rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBeginInfo.renderPass = s_renderPass;
    rpBeginInfo.framebuffer = s_framebuffers[index];
    rpBeginInfo.renderArea.extent = s_extent;
    rpBeginInfo.clearValueCount = 0;
    rpBeginInfo.pClearValues = nullptr;

    VkWrap::vkCmdBeginRenderPass_(cmd, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    CPU::Tick();

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)s_extent.width, (float)s_extent.height);
    io.DeltaTime = 1.0f / 60.0f;

    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();

    DrawMenu();

    Touch::SetMenuBounds(LastCoordinate.Pos_x, LastCoordinate.Pos_y, LastCoordinate.Size_x, LastCoordinate.Size_y);

    ImGui::EndFrame();
    ImGui::Render();

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    VkWrap::vkCmdEndRenderPass_(cmd);
    VkWrap::vkEndCommandBuffer_(cmd);

    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = pPresentInfo->waitSemaphoreCount;
    submitInfo.pWaitSemaphores = pPresentInfo->pWaitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    VkWrap::vkQueueSubmit_(queue, 1, &submitInfo, s_fences[index]);

    return orig_vkQueuePresentKHR(queue, pPresentInfo);
}

static VkResult hooked_vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain) {
    VkResult res = orig_vkCreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);
    if (res != VK_SUCCESS) return res;

    s_extent = pCreateInfo->imageExtent;
    s_format = pCreateInfo->imageFormat;

    uint32_t imageCount = 0;
    VkWrap::vkGetSwapchainImagesKHR_(device, *pSwapchain, &imageCount, nullptr);
    s_images.resize(imageCount);
    VkWrap::vkGetSwapchainImagesKHR_(device, *pSwapchain, &imageCount, s_images.data());

    s_imageViews.resize(imageCount);
    s_framebuffers.resize(imageCount);

    for (uint32_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = s_images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = s_format;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        VkWrap::vkCreateImageView_(device, &viewInfo, nullptr, &s_imageViews[i]);
    }

    if (s_renderPass == VK_NULL_HANDLE) {
        VkAttachmentDescription attachment{};
        attachment.format = s_format;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference color_attachment{};
        color_attachment.attachment = 0;
        color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo rpInfo{};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.attachmentCount = 1;
        rpInfo.pAttachments = &attachment;
        rpInfo.subpassCount = 1;
        rpInfo.pSubpasses = &subpass;
        rpInfo.dependencyCount = 1;
        rpInfo.pDependencies = &dependency;
        VkWrap::vkCreateRenderPass_(device, &rpInfo, nullptr, &s_renderPass);
    }

    for (uint32_t i = 0; i < imageCount; i++) {
        VkImageView attachments[] = { s_imageViews[i] };
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = s_renderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = attachments;
        fbInfo.width = s_extent.width;
        fbInfo.height = s_extent.height;
        fbInfo.layers = 1;
        VkWrap::vkCreateFramebuffer_(device, &fbInfo, nullptr, &s_framebuffers[i]);
    }

    if (s_descriptorPool == VK_NULL_HANDLE) {
        VkDescriptorPoolSize poolSizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 1000 * (uint32_t)(sizeof(poolSizes) / sizeof(poolSizes[0]));
        poolInfo.poolSizeCount = (uint32_t)(sizeof(poolSizes) / sizeof(poolSizes[0]));
        poolInfo.pPoolSizes = poolSizes;
        VkWrap::vkCreateDescriptorPool_(device, &poolInfo, nullptr, &s_descriptorPool);
    }

    if (s_cmdPool == VK_NULL_HANDLE) {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = s_queueFamily;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        VkWrap::vkCreateCommandPool_(device, &poolInfo, nullptr, &s_cmdPool);
    }

    if (s_cmdBuffers.empty()) {
        s_cmdBuffers.resize(imageCount);
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = s_cmdPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = imageCount;
        VkWrap::vkAllocateCommandBuffers_(device, &allocInfo, s_cmdBuffers.data());
    }

    if (s_fences.empty()) {
        s_fences.resize(imageCount);
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        for (uint32_t i = 0; i < imageCount; i++) {
            VkWrap::vkCreateFence_(device, &fenceInfo, nullptr, &s_fences[i]);
        }
    }

    if (!s_imguiInit) {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.Instance = s_instance;
        initInfo.PhysicalDevice = s_physicalDevice;
        initInfo.Device = s_device;
        initInfo.QueueFamily = s_queueFamily;
        initInfo.Queue = s_queue;
        initInfo.PipelineCache = VK_NULL_HANDLE;
        initInfo.DescriptorPool = s_descriptorPool;
        initInfo.MinImageCount = imageCount;
        initInfo.ImageCount = imageCount;
        initInfo.Allocator = nullptr;

        initInfo.RenderPass = s_renderPass;
        initInfo.UseDynamicRendering = false;

        ImGui_ImplVulkan_Init(&initInfo);

        Touch::Init({(float)s_extent.width, (float)s_extent.height}, true);
        InitMenuStyle();

        s_imguiInit = true;
        LOGI("VulkanGraphics: ImGui initialized successfully");
    }

    if (VkWrap::vkCreateSwapchainKHR_ && !orig_vkCreateSwapchainKHR) {
        DobbyHook((void*)VkWrap::vkCreateSwapchainKHR_, (void*)hooked_vkCreateSwapchainKHR, (void**)&orig_vkCreateSwapchainKHR);
    }
    if (VkWrap::vkQueuePresentKHR_ && !orig_vkQueuePresentKHR) {
        DobbyHook((void*)VkWrap::vkQueuePresentKHR_, (void*)hooked_vkQueuePresentKHR, (void**)&orig_vkQueuePresentKHR);
    }

    return res;
}

static VkResult hooked_vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice) {
    VkResult res = orig_vkCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
    if (res != VK_SUCCESS) return res;

    s_physicalDevice = physicalDevice;
    s_device = *pDevice;

    VkWrap::ResolveDeviceFunctions(s_device);

    vkGetDeviceQueue_ = (PFN_vkGetDeviceQueue)VkWrap::vkGetDeviceProcAddr_(s_device, "vkGetDeviceQueue");

    {
        uint32_t familyCount = 0;
        VkWrap::vkGetPhysicalDeviceQueueFamilyProperties_(physicalDevice, &familyCount, nullptr);
        std::vector<VkQueueFamilyProperties> familyProps(familyCount);
        VkWrap::vkGetPhysicalDeviceQueueFamilyProperties_(physicalDevice, &familyCount, familyProps.data());

        for (uint32_t i = 0; i < pCreateInfo->queueCreateInfoCount; i++) {
            uint32_t familyIndex = pCreateInfo->pQueueCreateInfos[i].queueFamilyIndex;
            if (familyIndex < familyCount && (familyProps[familyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                s_queueFamily = familyIndex;
                vkGetDeviceQueue_(s_device, s_queueFamily, 0, &s_queue);
                break;
            }
        }
    }

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
    return res == 0;
}

bool IsActive() {
    return s_active;
}

}
