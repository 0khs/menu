#define VK_USE_PLATFORM_ANDROID_KHR
#include <cstdlib>
#include <dlfcn.h>
#include "VulkanGraphics.h"
#include "imgui_impl_vulkan.h"
#include <vulkan/vulkan_android.h>
#include <android/native_window.h>
#include <unistd.h>
#include <android/log.h>

#define VK_LOG_TAG "VulkanGraphics"
#define VK_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, VK_LOG_TAG, __VA_ARGS__)
#define VK_LOGW(...) __android_log_print(ANDROID_LOG_WARN, VK_LOG_TAG, __VA_ARGS__)
#define VK_LOGI(...) __android_log_print(ANDROID_LOG_INFO, VK_LOG_TAG, __VA_ARGS__)

static void check_vk_result(VkResult err) {
    if (err == VK_SUCCESS) return;
    VK_LOGE("VkResult = %d", err);
}

static bool IsExtensionAvailable(const ImVector<VkExtensionProperties>& properties, const char* extension) {
    for (const VkExtensionProperties& p : properties)
        if (strcmp(p.extensionName, extension) == 0)
            return true;
    return false;
}

VkPhysicalDevice VulkanGraphics::SetupVulkan_SelectPhysicalDevice() {
    uint32_t gpu_count = 0;
    VkResult err = vkEnumeratePhysicalDevices(m_Instance, &gpu_count, nullptr);
    if (err != VK_SUCCESS || gpu_count == 0) {
        VK_LOGE("No Vulkan physical devices found (err=%d, count=%u)", err, gpu_count);
        return VK_NULL_HANDLE;
    }

    ImVector<VkPhysicalDevice> gpus;
    gpus.resize(gpu_count);
    err = vkEnumeratePhysicalDevices(m_Instance, &gpu_count, gpus.Data);
    if (err != VK_SUCCESS) {
        VK_LOGE("vkEnumeratePhysicalDevices failed (err=%d)", err);
        return VK_NULL_HANDLE;
    }

    for (VkPhysicalDevice& device : gpus) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            return device;
    }

    return gpus[0];
}

bool VulkanGraphics::Create() {
    if (InitVulkan() != 1) {
        VK_LOGE("Vulkan not supported: %s", dlerror() ? dlerror() : "unknown");
        return false;
    }

    wd = std::make_unique<ImGui_ImplVulkanH_Window>();

    void* libvulkan = dlopen("libvulkan.so", RTLD_NOW);
    if (!libvulkan) {
        VK_LOGE("Failed to open libvulkan.so");
        return false;
    }

    ImGui_ImplVulkan_LoadFunctions(0, [](const char* function_name, void* handle) -> PFN_vkVoidFunction {
        return reinterpret_cast<PFN_vkVoidFunction>(dlsym(handle, function_name));
    }, libvulkan);

    VkResult err;
    {
        const char* instance_extensions[] = {
            "VK_KHR_surface",
            "VK_KHR_android_surface",
        };
        uint32_t instance_extensions_count = sizeof(instance_extensions) / sizeof(instance_extensions[0]);
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "zxMenu";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "zxMenu";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_MAKE_VERSION(1, 1, 0);
        VkInstanceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &appInfo;
        create_info.enabledExtensionCount = instance_extensions_count;
        create_info.ppEnabledExtensionNames = instance_extensions;
        err = vkCreateInstance(&create_info, m_Allocator, &m_Instance);
        if (err != VK_SUCCESS) {
            VK_LOGE("vkCreateInstance failed: %d", err);
            return false;
        }
    }

    m_PhysicalDevice = SetupVulkan_SelectPhysicalDevice();
    if (m_PhysicalDevice == VK_NULL_HANDLE) {
        VK_LOGE("No suitable physical device found");
        vkDestroyInstance(m_Instance, m_Allocator);
        m_Instance = VK_NULL_HANDLE;
        return false;
    }

    {
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &count, nullptr);
        if (count == 0) {
            VK_LOGE("No queue families found");
            vkDestroyInstance(m_Instance, m_Allocator);
            m_Instance = VK_NULL_HANDLE;
            return false;
        }
        ImVector<VkQueueFamilyProperties> queues;
        queues.resize(count);
        vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &count, queues.Data);
        m_QueueFamily = (uint32_t)-1;
        for (uint32_t i = 0; i < count; i++) {
            if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                m_QueueFamily = i;
                break;
            }
        }
        if (m_QueueFamily == (uint32_t)-1) {
            VK_LOGE("No graphics queue family found");
            vkDestroyInstance(m_Instance, m_Allocator);
            m_Instance = VK_NULL_HANDLE;
            return false;
        }
    }

    {
        ImVector<const char*> device_extensions;
        device_extensions.push_back("VK_KHR_swapchain");

        uint32_t properties_count;
        ImVector<VkExtensionProperties> properties;
        vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &properties_count, nullptr);
        properties.resize(properties_count);
        vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &properties_count, properties.Data);

#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
        if (IsExtensionAvailable(properties, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
            device_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

        const float queue_priority[] = {1.0f};
        VkDeviceQueueCreateInfo queue_info[1] = {};
        queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info[0].queueFamilyIndex = m_QueueFamily;
        queue_info[0].queueCount = 1;
        queue_info[0].pQueuePriorities = queue_priority;
        VkDeviceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
        create_info.pQueueCreateInfos = queue_info;
        create_info.enabledExtensionCount = (uint32_t)device_extensions.Size;
        create_info.ppEnabledExtensionNames = device_extensions.Data;
        err = vkCreateDevice(m_PhysicalDevice, &create_info, m_Allocator, &m_Device);
        if (err != VK_SUCCESS) {
            VK_LOGE("vkCreateDevice failed: %d", err);
            vkDestroyInstance(m_Instance, m_Allocator);
            m_Instance = VK_NULL_HANDLE;
            m_PhysicalDevice = VK_NULL_HANDLE;
            return false;
        }
        vkGetDeviceQueue(m_Device, m_QueueFamily, 0, &m_Queue);
    }

    {
        VkDescriptorPoolSize pool_sizes[] = {
            {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
            {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
        pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        err = vkCreateDescriptorPool(m_Device, &pool_info, m_Allocator, &m_DescriptorPool);
        if (err != VK_SUCCESS) {
            VK_LOGE("vkCreateDescriptorPool failed: %d", err);
            Cleanup();
            return false;
        }
    }

    {
        VkSurfaceKHR surface;

        ANativeWindow_setBuffersGeometry(m_Window, 0, 0, WINDOW_FORMAT_RGBA_8888);

        VkAndroidSurfaceCreateInfoKHR createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
        createInfo.pNext = nullptr;
        createInfo.flags = 0;
        createInfo.window = m_Window;

        err = vkCreateAndroidSurfaceKHR(m_Instance, &createInfo, m_Allocator, &surface);
        if (err != VK_SUCCESS) {
            VK_LOGE("vkCreateAndroidSurfaceKHR failed: %d", err);
            Cleanup();
            return false;
        }
        wd->Surface = surface;

        VkBool32 res;
        err = vkGetPhysicalDeviceSurfaceSupportKHR(m_PhysicalDevice, m_QueueFamily, wd->Surface, &res);
        if (err != VK_SUCCESS || res != VK_TRUE) {
            VK_LOGE("No WSI support on physical device (err=%d, res=%d)", err, (int)res);
            Cleanup();
            return false;
        }

        VkSurfaceCapabilitiesKHR cap;
        err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, wd->Surface, &cap);
        if (err != VK_SUCCESS) {
            VK_LOGE("vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed: %d", err);
            Cleanup();
            return false;
        }

        uint32_t present_mode_count;
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, wd->Surface, &present_mode_count, nullptr);
        ImVector<VkPresentModeKHR> present_modes;
        present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, wd->Surface, &present_mode_count, present_modes.Data);

        VkPresentModeKHR best_present_mode = VK_PRESENT_MODE_FIFO_KHR;
        for (VkPresentModeKHR pm : present_modes) {
            if (pm == VK_PRESENT_MODE_MAILBOX_KHR) {
                best_present_mode = pm;
                break;
            }
            if (pm == VK_PRESENT_MODE_IMMEDIATE_KHR && best_present_mode == VK_PRESENT_MODE_FIFO_KHR) {
                best_present_mode = pm;
            }
        }

        uint32_t format_count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, wd->Surface, &format_count, nullptr);
        ImVector<VkSurfaceFormatKHR> formats;
        formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, wd->Surface, &format_count, formats.Data);

        VkSurfaceFormatKHR best_format = formats[0];
        for (const VkSurfaceFormatKHR& f : formats) {
            if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
                best_format = f;
                break;
            }
            if (f.format == VK_FORMAT_R8G8B8A8_UNORM && f.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
                best_format = f;
            }
        }

        wd->SurfaceFormat = best_format;
        wd->PresentMode = best_present_mode;

        int fb_width = (int)m_Width;
        int fb_height = (int)m_Height;
        if (fb_width < cap.minImageExtent.width) fb_width = cap.minImageExtent.width;
        if (fb_width > cap.maxImageExtent.width) fb_width = cap.maxImageExtent.width;
        if (fb_height < cap.minImageExtent.height) fb_height = cap.minImageExtent.height;
        if (fb_height > cap.maxImageExtent.height) fb_height = cap.maxImageExtent.height;

        m_MinImageCount = cap.minImageCount + 1;
        if (cap.maxImageCount > 0 && m_MinImageCount > cap.maxImageCount)
            m_MinImageCount = cap.maxImageCount;

        ImGui_ImplVulkanH_CreateOrResizeWindow(m_Instance, m_PhysicalDevice, m_Device, wd.get(), m_QueueFamily,
                                               m_Allocator,
                                               fb_width, fb_height, m_MinImageCount, 0);

        wd->FrameIndex = 0;

        VK_LOGI("swapchain: %dx%d images=%d present=%d format=%d colorspace=%d",
                wd->Width, wd->Height, wd->ImageCount, (int)best_present_mode, (int)best_format.format, (int)best_format.colorSpace);
    }

    return true;
}

void VulkanGraphics::Setup() {
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_Instance;
    init_info.PhysicalDevice = m_PhysicalDevice;
    init_info.Device = m_Device;
    init_info.QueueFamily = m_QueueFamily;
    init_info.Queue = m_Queue;
    init_info.PipelineCache = m_PipelineCache;
    init_info.DescriptorPool = m_DescriptorPool;
    init_info.PipelineInfoMain.RenderPass = wd->RenderPass;
    init_info.MinImageCount = m_MinImageCount;
    init_info.ImageCount = wd->ImageCount;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = m_Allocator;
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info);

    {
        VkCommandPool command_pool = wd->Frames[wd->FrameIndex].CommandPool;
        VkCommandBufferAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandPool = command_pool;
        alloc_info.commandBufferCount = 1;
        VkCommandBuffer command_buffer;
        vkAllocateCommandBuffers(m_Device, &alloc_info, &command_buffer);

        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(command_buffer, &begin_info);

        VkSubmitInfo end_info = {};
        end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        end_info.commandBufferCount = 1;
        end_info.pCommandBuffers = &command_buffer;
        vkEndCommandBuffer(command_buffer);
        vkQueueSubmit(m_Queue, 1, &end_info, VK_NULL_HANDLE);

        ImGuiIO& io = ImGui::GetIO();
        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        size_t upload_size = width * height * 4 * sizeof(char);

        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = upload_size;
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VkBuffer upload_buffer;
        vkCreateBuffer(m_Device, &buffer_info, m_Allocator, &upload_buffer);

        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(m_Device, upload_buffer, &req);

        VkMemoryAllocateInfo alloc_mem = {};
        alloc_mem.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_mem.allocationSize = req.size;
        alloc_mem.memoryTypeIndex = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        VkDeviceMemory upload_buffer_memory;
        vkAllocateMemory(m_Device, &alloc_mem, m_Allocator, &upload_buffer_memory);
        vkBindBufferMemory(m_Device, upload_buffer, upload_buffer_memory, 0);

        void* map = NULL;
        vkMapMemory(m_Device, upload_buffer_memory, 0, upload_size, 0, &map);
        memcpy(map, pixels, upload_size);
        VkMappedMemoryRange range[1] = {};
        range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range[0].memory = upload_buffer_memory;
        range[0].size = upload_size;
        vkFlushMappedMemoryRanges(m_Device, 1, range);
        vkUnmapMemory(m_Device, upload_buffer_memory);

        VkImageCreateInfo image_info = {};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
        image_info.extent.width = width;
        image_info.extent.height = height;
        image_info.extent.depth = 1;
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImage font_image;
        vkCreateImage(m_Device, &image_info, m_Allocator, &font_image);

        VkMemoryRequirements font_req;
        vkGetImageMemoryRequirements(m_Device, font_image, &font_req);
        VkMemoryAllocateInfo font_alloc = {};
        font_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        font_alloc.allocationSize = font_req.size;
        font_alloc.memoryTypeIndex = findMemoryType(font_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VkDeviceMemory font_memory;
        vkAllocateMemory(m_Device, &font_alloc, m_Allocator, &font_memory);
        vkBindImageMemory(m_Device, font_image, font_memory, 0);

        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = font_image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = VK_FORMAT_R8G8B8A8_UNORM;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.layerCount = 1;
        VkImageView font_view;
        vkCreateImageView(m_Device, &view_info, m_Allocator, &font_view);

        VkSamplerCreateInfo sampler_info = {};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.minLod = -1000;
        sampler_info.maxLod = 1000;
        sampler_info.maxAnisotropy = 1.0f;
        VkSampler font_sampler;
        vkCreateSampler(m_Device, &sampler_info, m_Allocator, &font_sampler);

        VkCommandBufferBeginInfo font_begin = {};
        font_begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        font_begin.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkResetCommandPool(m_Device, command_pool, 0);
        vkBeginCommandBuffer(command_buffer, &font_begin);

        VkImageMemoryBarrier copy_barrier = {};
        copy_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        copy_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        copy_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        copy_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copy_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        copy_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        copy_barrier.image = font_image;
        copy_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_barrier.subresourceRange.levelCount = 1;
        copy_barrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &copy_barrier);

        VkBufferImageCopy region = {};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent.width = width;
        region.imageExtent.height = height;
        region.imageExtent.depth = 1;
        vkCmdCopyBufferToImage(command_buffer, upload_buffer, font_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        VkImageMemoryBarrier use_barrier = {};
        use_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        use_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        use_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        use_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        use_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        use_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        use_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        use_barrier.image = font_image;
        use_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        use_barrier.subresourceRange.levelCount = 1;
        use_barrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &use_barrier);

        VkSubmitInfo font_end = {};
        font_end.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        font_end.commandBufferCount = 1;
        font_end.pCommandBuffers = &command_buffer;
        vkEndCommandBuffer(command_buffer);
        vkQueueSubmit(m_Queue, 1, &font_end, VK_NULL_HANDLE);

        vkDeviceWaitIdle(m_Device);

        VkDescriptorSet font_descriptor_set = ImGui_ImplVulkan_AddTexture(font_sampler, font_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        io.Fonts->SetTexID((ImTextureID)font_descriptor_set);

        m_FontResources.image = font_image;
        m_FontResources.memory = font_memory;
        m_FontResources.view = font_view;
        m_FontResources.sampler = font_sampler;
        m_FontResources.descriptorSet = font_descriptor_set;

        vkDestroyBuffer(m_Device, upload_buffer, m_Allocator);
        vkFreeMemory(m_Device, upload_buffer_memory, m_Allocator);
    }
}

void VulkanGraphics::PrepareFrame(bool resize) {
    if (m_SwapChainRebuild || resize) {
        int width = ANativeWindow_getWidth(m_Window);
        int height = ANativeWindow_getHeight(m_Window);

        if (width > 0 && height > 0) {
            if (width != m_LastWidth || height != m_LastHeight || m_SwapChainRebuild) {
                m_LastWidth = width;
                m_LastHeight = height;

                vkDeviceWaitIdle(m_Device);

                ImGui_ImplVulkan_SetMinImageCount(m_MinImageCount);
                ImGui_ImplVulkanH_CreateOrResizeWindow(m_Instance, m_PhysicalDevice, m_Device, wd.get(),
                                                       m_QueueFamily, m_Allocator, width, height, m_MinImageCount, 0);
                wd->FrameIndex = 0;
                m_SwapChainRebuild = false;
            }
        }
    }
    ImGui_ImplVulkan_NewFrame();
}

void VulkanGraphics::Render(ImDrawData* drawData) {
    if (!wd || !m_Device || wd->SemaphoreCount == 0) return;

    VkResult err;

    VkSemaphore image_acquired_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;

    err = vkAcquireNextImageKHR(m_Device, wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
        m_SwapChainRebuild = true;
        return;
    }
    if (err == VK_TIMEOUT) {
        VK_LOGW("vkAcquireNextImageKHR timeout, rebuilding swapchain");
        m_SwapChainRebuild = true;
        return;
    }
    if (err == VK_ERROR_SURFACE_LOST_KHR) {
        VK_LOGW("vkAcquireNextImageKHR surface lost, rebuilding swapchain");
        m_SwapChainRebuild = true;
        return;
    }
    if (err != VK_SUCCESS) {
        VK_LOGE("vkAcquireNextImageKHR failed: %d, rebuilding swapchain", err);
        m_SwapChainRebuild = true;
        return;
    }

    ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
    {
        err = vkWaitForFences(m_Device, 1, &fd->Fence, VK_TRUE, UINT64_MAX);
        if (err != VK_SUCCESS) { VK_LOGE("vkWaitForFences failed: %d", err); m_SwapChainRebuild = true; return; }

        err = vkResetFences(m_Device, 1, &fd->Fence);
        if (err != VK_SUCCESS) { VK_LOGE("vkResetFences failed: %d", err); m_SwapChainRebuild = true; return; }
    }
    {
        err = vkResetCommandPool(m_Device, fd->CommandPool, 0);
        if (err != VK_SUCCESS) { VK_LOGE("vkResetCommandPool failed: %d", err); m_SwapChainRebuild = true; return; }
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
        if (err != VK_SUCCESS) { VK_LOGE("vkBeginCommandBuffer failed: %d", err); m_SwapChainRebuild = true; return; }
    }
    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = wd->RenderPass;
        info.framebuffer = fd->Framebuffer;
        info.renderArea.extent.width = wd->Width;
        info.renderArea.extent.height = wd->Height;
        info.clearValueCount = 1;
        info.pClearValues = &wd->ClearValue;
        vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    ImGui_ImplVulkan_RenderDrawData(drawData, fd->CommandBuffer);

    vkCmdEndRenderPass(fd->CommandBuffer);
    {
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &image_acquired_semaphore;
        info.pWaitDstStageMask = &wait_stage;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &fd->CommandBuffer;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores = &render_complete_semaphore;

        err = vkEndCommandBuffer(fd->CommandBuffer);
        if (err != VK_SUCCESS) { VK_LOGE("vkEndCommandBuffer failed: %d", err); m_SwapChainRebuild = true; return; }
        err = vkQueueSubmit(m_Queue, 1, &info, fd->Fence);
        if (err != VK_SUCCESS) { VK_LOGE("vkQueueSubmit failed: %d", err); m_SwapChainRebuild = true; return; }
    }

    {
        if (m_SwapChainRebuild) return;
        VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
        VkPresentInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &render_complete_semaphore;
        info.swapchainCount = 1;
        info.pSwapchains = &wd->Swapchain;
        info.pImageIndices = &wd->FrameIndex;
        VkResult present_err = vkQueuePresentKHR(m_Queue, &info);
        if (present_err == VK_ERROR_OUT_OF_DATE_KHR || present_err == VK_SUBOPTIMAL_KHR) {
            m_SwapChainRebuild = true;
            return;
        }
        if (present_err != VK_SUCCESS) {
            VK_LOGE("vkQueuePresentKHR failed: %d", present_err);
            m_SwapChainRebuild = true;
            return;
        }
        wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount;
    }
}

void VulkanGraphics::PrepareShutdown() {
    if (!m_Device) return;
    vkDeviceWaitIdle(m_Device);
    ImGui_ImplVulkan_Shutdown();
}

void VulkanGraphics::Cleanup() {
    if (m_Device) {
        vkDeviceWaitIdle(m_Device);
    }

    if (m_FontResources.image) {
        if (m_FontResources.descriptorSet)
            ImGui_ImplVulkan_RemoveTexture(m_FontResources.descriptorSet);
        if (m_FontResources.sampler)
            vkDestroySampler(m_Device, m_FontResources.sampler, m_Allocator);
        if (m_FontResources.view)
            vkDestroyImageView(m_Device, m_FontResources.view, m_Allocator);
        if (m_FontResources.image)
            vkDestroyImage(m_Device, m_FontResources.image, m_Allocator);
        if (m_FontResources.memory)
            vkFreeMemory(m_Device, m_FontResources.memory, m_Allocator);
        m_FontResources = {};
    }

    if (wd) {
        ImGui_ImplVulkanH_DestroyWindow(m_Instance, m_Device, wd.get(), m_Allocator);
    }
    wd.reset();

    if (m_DescriptorPool != VK_NULL_HANDLE && m_Device != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_Device, m_DescriptorPool, m_Allocator);
        m_DescriptorPool = VK_NULL_HANDLE;
    }

    if (m_Device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_Device, m_Allocator);
        m_Device = VK_NULL_HANDLE;
    }

    if (m_Instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_Instance, m_Allocator);
        m_Instance = VK_NULL_HANDLE;
    }

    m_PhysicalDevice = VK_NULL_HANDLE;
    m_Queue = VK_NULL_HANDLE;
    m_QueueFamily = (uint32_t)-1;
    m_DebugReport = VK_NULL_HANDLE;
    m_PipelineCache = VK_NULL_HANDLE;
    m_LastWidth = 0;
    m_LastHeight = 0;
    m_SwapChainRebuild = false;
}

uint32_t VulkanGraphics::findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++)
        if ((type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;

    return 0xFFFFFFFF;
}

BaseTexData* VulkanGraphics::LoadTexture(BaseTexData* tex, void* pixel_data) {
    if (!m_Device || !wd || wd->FrameIndex >= (uint32_t)wd->ImageCount) {
        VK_LOGE("Cannot load texture: device not ready");
        return nullptr;
    }

    auto* tex_data = new VulkanTextureData();
    tex_data->Width = tex->Width;
    tex_data->Height = tex->Height;
    tex_data->Channels = tex->Channels;

    size_t image_size = tex_data->Width * tex_data->Height * tex_data->Channels;

    VkResult err;
    {
        VkImageCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = VK_FORMAT_R8G8B8A8_UNORM;
        info.extent.width = tex_data->Width;
        info.extent.height = tex_data->Height;
        info.extent.depth = 1;
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        err = vkCreateImage(m_Device, &info, m_Allocator, &tex_data->Image);
        if (err != VK_SUCCESS) { delete tex_data; return nullptr; }
        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(m_Device, tex_data->Image, &req);
        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = req.size;
        alloc_info.memoryTypeIndex = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (alloc_info.memoryTypeIndex == 0xFFFFFFFF) {
            vkDestroyImage(m_Device, tex_data->Image, m_Allocator);
            delete tex_data;
            return nullptr;
        }
        err = vkAllocateMemory(m_Device, &alloc_info, m_Allocator, &tex_data->ImageMemory);
        if (err != VK_SUCCESS) { vkDestroyImage(m_Device, tex_data->Image, m_Allocator); delete tex_data; return nullptr; }
        err = vkBindImageMemory(m_Device, tex_data->Image, tex_data->ImageMemory, 0);
        if (err != VK_SUCCESS) {
            vkFreeMemory(m_Device, tex_data->ImageMemory, m_Allocator);
            vkDestroyImage(m_Device, tex_data->Image, m_Allocator);
            delete tex_data;
            return nullptr;
        }
    }

    {
        VkImageViewCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image = tex_data->Image;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = VK_FORMAT_R8G8B8A8_UNORM;
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.layerCount = 1;
        err = vkCreateImageView(m_Device, &info, m_Allocator, &tex_data->ImageView);
        if (err != VK_SUCCESS) {
            vkFreeMemory(m_Device, tex_data->ImageMemory, m_Allocator);
            vkDestroyImage(m_Device, tex_data->Image, m_Allocator);
            delete tex_data;
            return nullptr;
        }
    }

    {
        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.minLod = -1000;
        sampler_info.maxLod = 1000;
        sampler_info.maxAnisotropy = 1.0f;
        err = vkCreateSampler(m_Device, &sampler_info, m_Allocator, &tex_data->Sampler);
        if (err != VK_SUCCESS) {
            vkDestroyImageView(m_Device, tex_data->ImageView, m_Allocator);
            vkFreeMemory(m_Device, tex_data->ImageMemory, m_Allocator);
            vkDestroyImage(m_Device, tex_data->Image, m_Allocator);
            delete tex_data;
            return nullptr;
        }
    }

    tex_data->DS = (void*)ImGui_ImplVulkan_AddTexture(tex_data->Sampler, tex_data->ImageView,
                                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    {
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = image_size;
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        err = vkCreateBuffer(m_Device, &buffer_info, m_Allocator, &tex_data->UploadBuffer);
        if (err != VK_SUCCESS) { RemoveTexture(tex_data); return nullptr; }
        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(m_Device, tex_data->UploadBuffer, &req);
        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = req.size;
        alloc_info.memoryTypeIndex = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        if (alloc_info.memoryTypeIndex == 0xFFFFFFFF) { RemoveTexture(tex_data); return nullptr; }
        err = vkAllocateMemory(m_Device, &alloc_info, m_Allocator, &tex_data->UploadBufferMemory);
        if (err != VK_SUCCESS) { RemoveTexture(tex_data); return nullptr; }
        err = vkBindBufferMemory(m_Device, tex_data->UploadBuffer, tex_data->UploadBufferMemory, 0);
        if (err != VK_SUCCESS) { RemoveTexture(tex_data); return nullptr; }
    }

    {
        void* map = NULL;
        err = vkMapMemory(m_Device, tex_data->UploadBufferMemory, 0, image_size, 0, &map);
        if (err != VK_SUCCESS) { RemoveTexture(tex_data); return nullptr; }
        memcpy(map, pixel_data, image_size);
        VkMappedMemoryRange range[1] = {};
        range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range[0].memory = tex_data->UploadBufferMemory;
        range[0].size = image_size;
        err = vkFlushMappedMemoryRanges(m_Device, 1, range);
        vkUnmapMemory(m_Device, tex_data->UploadBufferMemory);
        if (err != VK_SUCCESS) { RemoveTexture(tex_data); return nullptr; }
    }

    VkCommandPool command_pool = wd->Frames[wd->FrameIndex].CommandPool;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    {
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandPool = command_pool;
        alloc_info.commandBufferCount = 1;

        err = vkAllocateCommandBuffers(m_Device, &alloc_info, &command_buffer);
        if (err != VK_SUCCESS) { RemoveTexture(tex_data); return nullptr; }

        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(command_buffer, &begin_info);
        if (err != VK_SUCCESS) { vkFreeCommandBuffers(m_Device, command_pool, 1, &command_buffer); RemoveTexture(tex_data); return nullptr; }
    }

    {
        VkImageMemoryBarrier copy_barrier[1] = {};
        copy_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        copy_barrier[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        copy_barrier[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        copy_barrier[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copy_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        copy_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        copy_barrier[0].image = tex_data->Image;
        copy_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_barrier[0].subresourceRange.levelCount = 1;
        copy_barrier[0].subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL,
                             0, NULL, 1, copy_barrier);

        VkBufferImageCopy region = {};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent.width = tex_data->Width;
        region.imageExtent.height = tex_data->Height;
        region.imageExtent.depth = 1;
        vkCmdCopyBufferToImage(command_buffer, tex_data->UploadBuffer, tex_data->Image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        VkImageMemoryBarrier use_barrier[1] = {};
        use_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        use_barrier[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        use_barrier[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        use_barrier[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        use_barrier[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        use_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        use_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        use_barrier[0].image = tex_data->Image;
        use_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        use_barrier[0].subresourceRange.levelCount = 1;
        use_barrier[0].subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, NULL, 0, NULL, 1, use_barrier);
    }

    {
        VkSubmitInfo end_info = {};
        end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        end_info.commandBufferCount = 1;
        end_info.pCommandBuffers = &command_buffer;
        err = vkEndCommandBuffer(command_buffer);
        if (err != VK_SUCCESS) { vkFreeCommandBuffers(m_Device, command_pool, 1, &command_buffer); RemoveTexture(tex_data); return nullptr; }
        err = vkQueueSubmit(m_Queue, 1, &end_info, VK_NULL_HANDLE);
        if (err != VK_SUCCESS) { vkFreeCommandBuffers(m_Device, command_pool, 1, &command_buffer); RemoveTexture(tex_data); return nullptr; }
        err = vkDeviceWaitIdle(m_Device);
        vkFreeCommandBuffers(m_Device, command_pool, 1, &command_buffer);
        if (err != VK_SUCCESS) { RemoveTexture(tex_data); return nullptr; }
    }

    return tex_data;
}

void VulkanGraphics::RemoveTexture(BaseTexData* tex) {
    if (!tex || !m_Device) return;
    auto* tex_data = (VulkanTextureData*)(tex);
    if (tex_data->UploadBufferMemory != VK_NULL_HANDLE)
        vkFreeMemory(m_Device, tex_data->UploadBufferMemory, nullptr);
    if (tex_data->UploadBuffer != VK_NULL_HANDLE)
        vkDestroyBuffer(m_Device, tex_data->UploadBuffer, nullptr);
    if (tex_data->Sampler != VK_NULL_HANDLE)
        vkDestroySampler(m_Device, tex_data->Sampler, nullptr);
    if (tex_data->ImageView != VK_NULL_HANDLE)
        vkDestroyImageView(m_Device, tex_data->ImageView, nullptr);
    if (tex_data->Image != VK_NULL_HANDLE)
        vkDestroyImage(m_Device, tex_data->Image, nullptr);
    if (tex_data->ImageMemory != VK_NULL_HANDLE)
        vkFreeMemory(m_Device, tex_data->ImageMemory, nullptr);
    if (tex_data->DS)
        ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)tex_data->DS);
    delete tex_data;
}
