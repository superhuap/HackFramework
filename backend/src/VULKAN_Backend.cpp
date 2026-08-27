//
// Created by superhuap on 2026/8/25.
//

#include "backend/VULKAN_Backend.h"
#include "backend/MinHookHelper.h"

#include <vulkan/vulkan.h>

#include <vector>

#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_win32.h>

#include "menu/Menu.h"
#include "utils/InputHook.h"
#include "utils/Logger.h"

namespace
{

    VkAllocationCallbacks* g_allocator = nullptr;
    VkInstance g_instance = VK_NULL_HANDLE;
    VkPhysicalDevice g_physicalDevice = VK_NULL_HANDLE;
    VkDevice g_fakeDevice = VK_NULL_HANDLE;
    VkDevice g_device = VK_NULL_HANDLE;

    uint32_t g_queueFamily = static_cast<uint32_t>(-1);
    std::vector<VkQueueFamilyProperties> g_queueFamilies;

    VkPipelineCache g_pipelineCache = VK_NULL_HANDLE;
    VkDescriptorPool g_descriptorPool = VK_NULL_HANDLE;
    uint32_t g_minImageCount = 2;
    VkRenderPass g_renderPass = VK_NULL_HANDLE;
    ImGui_ImplVulkanH_Frame g_frames[8] = {};
    ImGui_ImplVulkanH_FrameSemaphores g_frameSemaphores[8] = {};

    HWND g_hwnd = nullptr;
    VkExtent2D g_imageExtent = {};

    void CleanupRenderTarget();
    void CleanupDeviceVulkan();
    void RenderImGui_Vulkan(VkQueue queue, const VkPresentInfoKHR* pPresentInfo);
    bool DoesQueueSupportGraphic(VkQueue queue, VkQueue* pGraphicQueue);

    bool CreateDeviceVK()
    {
        VkInstanceCreateInfo instanceInfo = {};
        constexpr const char* instanceExtension = "VK_KHR_surface";
        instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceInfo.enabledExtensionCount = 1;
        instanceInfo.ppEnabledExtensionNames = &instanceExtension;

        if (vkCreateInstance(&instanceInfo, g_allocator, &g_instance) != VK_SUCCESS)
        {
            LOG_ERROR("vkCreateInstance() failed");
            return false;
        }
        LOG_INFO("Vulkan: g_Instance: {}", reinterpret_cast<void*>(g_instance));

        uint32_t gpuCount = 0;
        vkEnumeratePhysicalDevices(g_instance, &gpuCount, nullptr);
        if (gpuCount == 0)
        {
            LOG_ERROR("No physical device found");
            return false;
        }

        std::vector<VkPhysicalDevice> gpus(gpuCount);
        vkEnumeratePhysicalDevices(g_instance, &gpuCount, gpus.data());

        uint32_t useGpu = 0;
        for (uint32_t i = 0; i < gpuCount; ++i)
        {
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(gpus[i], &properties);
            if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                useGpu = i;
                break;
            }
        }
        g_physicalDevice = gpus[useGpu];
        LOG_INFO("Vulkan: g_PhysicalDevice: {}", reinterpret_cast<void*>(g_physicalDevice));

        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(g_physicalDevice, &count, nullptr);
        g_queueFamilies.resize(count);
        vkGetPhysicalDeviceQueueFamilyProperties(g_physicalDevice, &count, g_queueFamilies.data());
        for (uint32_t i = 0; i < count; ++i)
        {
            if (g_queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                g_queueFamily = i;
                break;
            }
        }
        if (g_queueFamily == static_cast<uint32_t>(-1))
        {
            LOG_ERROR("No graphics queue family found");
            return false;
        }
        LOG_INFO("Vulkan: g_QueueFamily: {}", g_queueFamily);

        constexpr const char* deviceExtension = "VK_KHR_swapchain";
        constexpr float queuePriority = 1.0f;

        VkDeviceQueueCreateInfo queueInfo = {};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = g_queueFamily;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;

        VkDeviceCreateInfo deviceInfo = {};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.enabledExtensionCount = 1;
        deviceInfo.ppEnabledExtensionNames = &deviceExtension;

        if (vkCreateDevice(g_physicalDevice, &deviceInfo, g_allocator, &g_fakeDevice) != VK_SUCCESS)
        {
            LOG_ERROR("vkCreateDevice() failed");
            return false;
        }
        LOG_INFO("Vulkan: g_FakeDevice: {}", reinterpret_cast<void*>(g_fakeDevice));

        return true;
    }

    void CreateRenderTarget(VkDevice device, VkSwapchainKHR swapchain)
    {
        uint32_t imageCount = 0;
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);

        VkImage backbuffers[8] = {};
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, backbuffers);

        for (uint32_t i = 0; i < imageCount; ++i)
        {
            g_frames[i].Backbuffer = backbuffers[i];

            ImGui_ImplVulkanH_Frame* fd = &g_frames[i];
            ImGui_ImplVulkanH_FrameSemaphores* fsd = &g_frameSemaphores[i];
            {
                VkCommandPoolCreateInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
                info.queueFamilyIndex = g_queueFamily;
                vkCreateCommandPool(device, &info, g_allocator, &fd->CommandPool);
            }
            {
                VkCommandBufferAllocateInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                info.commandPool = fd->CommandPool;
                info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                info.commandBufferCount = 1;
                vkAllocateCommandBuffers(device, &info, &fd->CommandBuffer);
            }
            {
                VkFenceCreateInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
                vkCreateFence(device, &info, g_allocator, &fd->Fence);
            }
            {
                VkSemaphoreCreateInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
                vkCreateSemaphore(device, &info, g_allocator, &fsd->ImageAcquiredSemaphore);
                vkCreateSemaphore(device, &info, g_allocator, &fsd->RenderCompleteSemaphore);
            }
        }

        {
            VkAttachmentDescription attachment = {};
            attachment.format = VK_FORMAT_B8G8R8A8_UNORM;
            attachment.samples = VK_SAMPLE_COUNT_1_BIT;
            attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            VkAttachmentReference colorAttachment = {};
            colorAttachment.attachment = 0;
            colorAttachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkSubpassDescription subpass = {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorAttachment;

            VkRenderPassCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            info.attachmentCount = 1;
            info.pAttachments = &attachment;
            info.subpassCount = 1;
            info.pSubpasses = &subpass;

            vkCreateRenderPass(device, &info, g_allocator, &g_renderPass);
        }

        {
            VkImageViewCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            info.format = VK_FORMAT_B8G8R8A8_UNORM;

            info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            info.subresourceRange.baseMipLevel = 0;
            info.subresourceRange.levelCount = 1;
            info.subresourceRange.baseArrayLayer = 0;
            info.subresourceRange.layerCount = 1;

            for (uint32_t i = 0; i < imageCount; ++i)
            {
                ImGui_ImplVulkanH_Frame* fd = &g_frames[i];
                info.image = fd->Backbuffer;
                vkCreateImageView(device, &info, g_allocator, &fd->BackbufferView);
            }
        }

        {
            VkImageView attachment[1];
            VkFramebufferCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            info.renderPass = g_renderPass;
            info.attachmentCount = 1;
            info.pAttachments = attachment;
            info.width = (g_imageExtent.width == 0) ? 1920 : g_imageExtent.width;
            info.height = (g_imageExtent.height == 0) ? 1080 : g_imageExtent.height;
            info.layers = 1;

            for (uint32_t i = 0; i < imageCount; ++i)
            {
                ImGui_ImplVulkanH_Frame* fd = &g_frames[i];
                attachment[0] = fd->BackbufferView;
                vkCreateFramebuffer(device, &info, g_allocator, &fd->Framebuffer);
            }
        }

        if (!g_descriptorPool)
        {
            constexpr VkDescriptorPoolSize poolSizes[] = {
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
                {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

            VkDescriptorPoolCreateInfo poolInfo = {};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            poolInfo.maxSets = 1000 * IM_ARRAYSIZE(poolSizes);
            poolInfo.poolSizeCount = static_cast<uint32_t>(IM_ARRAYSIZE(poolSizes));
            poolInfo.pPoolSizes = poolSizes;

            vkCreateDescriptorPool(device, &poolInfo, g_allocator, &g_descriptorPool);
        }
    }

    bool DoesQueueSupportGraphic(VkQueue queue, VkQueue* pGraphicQueue)
    {
        for (uint32_t i = 0; i < g_queueFamilies.size(); ++i)
        {
            const VkQueueFamilyProperties& family = g_queueFamilies[i];
            for (uint32_t j = 0; j < family.queueCount; ++j)
            {
                VkQueue it = VK_NULL_HANDLE;
                vkGetDeviceQueue(g_device, i, j, &it);

                if (pGraphicQueue && (family.queueFlags & VK_QUEUE_GRAPHICS_BIT))
                {
                    if (*pGraphicQueue == VK_NULL_HANDLE)
                        *pGraphicQueue = it;
                }

                if (queue == it && (family.queueFlags & VK_QUEUE_GRAPHICS_BIT))
                    return true;
            }
        }

        return false;
    }

    VkResult(VKAPI_CALL* oAcquireNextImageKHR)(VkDevice, VkSwapchainKHR, uint64_t, VkSemaphore, VkFence, uint32_t*) = nullptr;
    VkResult VKAPI_CALL Hook_AcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout,
                                                  VkSemaphore semaphore, VkFence fence, uint32_t* pImageIndex)
    {
        g_device = device;
        return oAcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, pImageIndex);
    }

    VkResult(VKAPI_CALL* oAcquireNextImage2KHR)(VkDevice, const VkAcquireNextImageInfoKHR*, uint32_t*) = nullptr;
    VkResult VKAPI_CALL Hook_AcquireNextImage2KHR(VkDevice device, const VkAcquireNextImageInfoKHR* pAcquireInfo,
                                                   uint32_t* pImageIndex)
    {
        g_device = device;
        return oAcquireNextImage2KHR(device, pAcquireInfo, pImageIndex);
    }

    VkResult(VKAPI_CALL* oQueuePresentKHR)(VkQueue, const VkPresentInfoKHR*) = nullptr;
    VkResult VKAPI_CALL Hook_QueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo)
    {
        RenderImGui_Vulkan(queue, pPresentInfo);
        return oQueuePresentKHR(queue, pPresentInfo);
    }

    VkResult(VKAPI_CALL* oCreateSwapchainKHR)(VkDevice, const VkSwapchainCreateInfoKHR*, const VkAllocationCallbacks*, VkSwapchainKHR*) = nullptr;
    VkResult VKAPI_CALL Hook_CreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo,
                                                 const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain)
    {
        CleanupRenderTarget();
        g_imageExtent = pCreateInfo->imageExtent;
        return oCreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);
    }

    void CleanupRenderTarget()
    {
        for (uint32_t i = 0; i < RTL_NUMBER_OF(g_frames); ++i)
        {
            if (g_frames[i].Fence)
            {
                vkDestroyFence(g_device, g_frames[i].Fence, g_allocator);
                g_frames[i].Fence = VK_NULL_HANDLE;
            }
            if (g_frames[i].CommandBuffer)
            {
                vkFreeCommandBuffers(g_device, g_frames[i].CommandPool, 1, &g_frames[i].CommandBuffer);
                g_frames[i].CommandBuffer = VK_NULL_HANDLE;
            }
            if (g_frames[i].CommandPool)
            {
                vkDestroyCommandPool(g_device, g_frames[i].CommandPool, g_allocator);
                g_frames[i].CommandPool = VK_NULL_HANDLE;
            }
            if (g_frames[i].BackbufferView)
            {
                vkDestroyImageView(g_device, g_frames[i].BackbufferView, g_allocator);
                g_frames[i].BackbufferView = VK_NULL_HANDLE;
            }
            if (g_frames[i].Framebuffer)
            {
                vkDestroyFramebuffer(g_device, g_frames[i].Framebuffer, g_allocator);
                g_frames[i].Framebuffer = VK_NULL_HANDLE;
            }
        }

        for (uint32_t i = 0; i < RTL_NUMBER_OF(g_frameSemaphores); ++i)
        {
            if (g_frameSemaphores[i].ImageAcquiredSemaphore)
            {
                vkDestroySemaphore(g_device, g_frameSemaphores[i].ImageAcquiredSemaphore, g_allocator);
                g_frameSemaphores[i].ImageAcquiredSemaphore = VK_NULL_HANDLE;
            }
            if (g_frameSemaphores[i].RenderCompleteSemaphore)
            {
                vkDestroySemaphore(g_device, g_frameSemaphores[i].RenderCompleteSemaphore, g_allocator);
                g_frameSemaphores[i].RenderCompleteSemaphore = VK_NULL_HANDLE;
            }
        }
    }

    void CleanupDeviceVulkan()
    {
        CleanupRenderTarget();

        if (g_descriptorPool)
        {
            vkDestroyDescriptorPool(g_device, g_descriptorPool, g_allocator);
            g_descriptorPool = VK_NULL_HANDLE;
        }
        if (g_instance)
        {
            vkDestroyInstance(g_instance, g_allocator);
            g_instance = VK_NULL_HANDLE;
        }

        g_imageExtent = {};
        g_device = VK_NULL_HANDLE;
    }

    void RenderImGui_Vulkan(VkQueue queue, const VkPresentInfoKHR* pPresentInfo)
    {
        if (!g_device || Utils::Input::shutting_down.load())
            return;

        VkQueue graphicQueue = VK_NULL_HANDLE;
        const bool queueSupportsGraphic = DoesQueueSupportGraphic(queue, &graphicQueue);

        menu::Menu::GetInstance().Initialize(g_hwnd);

        for (uint32_t i = 0; i < pPresentInfo->swapchainCount; ++i)
        {
            VkSwapchainKHR swapchain = pPresentInfo->pSwapchains[i];
            if (g_frames[0].Framebuffer == VK_NULL_HANDLE)
                CreateRenderTarget(g_device, swapchain);

            ImGui_ImplVulkanH_Frame* fd = &g_frames[pPresentInfo->pImageIndices[i]];
            ImGui_ImplVulkanH_FrameSemaphores* fsd = &g_frameSemaphores[pPresentInfo->pImageIndices[i]];
            {
                vkWaitForFences(g_device, 1, &fd->Fence, VK_TRUE, ~0ull);
                vkResetFences(g_device, 1, &fd->Fence);
            }
            {
                vkResetCommandBuffer(fd->CommandBuffer, 0);

                VkCommandBufferBeginInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                vkBeginCommandBuffer(fd->CommandBuffer, &info);
            }
            {
                VkRenderPassBeginInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                info.renderPass = g_renderPass;
                info.framebuffer = fd->Framebuffer;
                if (g_imageExtent.width == 0 || g_imageExtent.height == 0)
                {
                    info.renderArea.extent.width = 3840;
                    info.renderArea.extent.height = 2160;
                }
                else
                {
                    info.renderArea.extent = g_imageExtent;
                }
                vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
            }

            if (!ImGui::GetIO().BackendRendererUserData)
            {
                ImGui_ImplVulkan_InitInfo init_info = {};
                init_info.ApiVersion = VK_API_VERSION_1_0;
                init_info.Instance = g_instance;
                init_info.PhysicalDevice = g_physicalDevice;
                init_info.Device = g_device;
                init_info.QueueFamily = g_queueFamily;
                init_info.Queue = graphicQueue;
                init_info.PipelineCache = g_pipelineCache;
                init_info.DescriptorPool = g_descriptorPool;
                init_info.MinImageCount = g_minImageCount;
                init_info.ImageCount = g_minImageCount;
                init_info.Allocator = g_allocator;

                init_info.PipelineInfoMain.RenderPass = g_renderPass;
                init_info.PipelineInfoMain.Subpass = 0;
                init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
                ImGui_ImplVulkan_Init(&init_info);
            }

            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            menu::Menu::GetInstance().Render();

            ImGui::Render();

            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), fd->CommandBuffer);

            vkCmdEndRenderPass(fd->CommandBuffer);
            vkEndCommandBuffer(fd->CommandBuffer);

            const uint32_t waitSemaphoresCount = i == 0 ? pPresentInfo->waitSemaphoreCount : 0;
            if (waitSemaphoresCount == 0 && !queueSupportsGraphic)
            {
                constexpr VkPipelineStageFlags stagesWait = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
                {
                    VkSubmitInfo info = {};
                    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                    info.pWaitDstStageMask = &stagesWait;
                    info.signalSemaphoreCount = 1;
                    info.pSignalSemaphores = &fsd->RenderCompleteSemaphore;
                    vkQueueSubmit(queue, 1, &info, VK_NULL_HANDLE);
                }
                {
                    VkSubmitInfo info = {};
                    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                    info.commandBufferCount = 1;
                    info.pCommandBuffers = &fd->CommandBuffer;
                    info.pWaitDstStageMask = &stagesWait;
                    info.waitSemaphoreCount = 1;
                    info.pWaitSemaphores = &fsd->RenderCompleteSemaphore;
                    info.signalSemaphoreCount = 1;
                    info.pSignalSemaphores = &fsd->ImageAcquiredSemaphore;
                    vkQueueSubmit(graphicQueue, 1, &info, fd->Fence);
                }
            }
            else
            {
                std::vector<VkPipelineStageFlags> stagesWait(waitSemaphoresCount,
                                                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

                VkSubmitInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                info.commandBufferCount = 1;
                info.pCommandBuffers = &fd->CommandBuffer;
                info.pWaitDstStageMask = stagesWait.data();
                info.waitSemaphoreCount = waitSemaphoresCount;
                info.pWaitSemaphores = pPresentInfo->pWaitSemaphores;
                info.signalSemaphoreCount = 1;
                info.pSignalSemaphores = &fsd->ImageAcquiredSemaphore;
                vkQueueSubmit(graphicQueue, 1, &info, fd->Fence);
            }
        }
    }

} // namespace

bool VULKAN_Backend::Initialize(HWND hWnd)
{
    g_hwnd = hWnd;

    if (!CreateDeviceVK())
    {
        LOG_ERROR("CreateDeviceVK() failed");
        return false;
    }

    auto getProc = [&](const char* name) -> void* {
        return reinterpret_cast<void*>(vkGetDeviceProcAddr(g_fakeDevice, name));
    };

    void* fnAcquireNextImageKHR = getProc("vkAcquireNextImageKHR");
    void* fnAcquireNextImage2KHR = getProc("vkAcquireNextImage2KHR");
    void* fnQueuePresentKHR = getProc("vkQueuePresentKHR");
    void* fnCreateSwapchainKHR = getProc("vkCreateSwapchainKHR");

    if (g_fakeDevice)
    {
        vkDestroyDevice(g_fakeDevice, g_allocator);
        g_fakeDevice = VK_NULL_HANDLE;
    }

    if (fnAcquireNextImageKHR)
    {
        g_hwnd = hWnd;

        LOG_INFO("Vulkan: fnAcquireNextImageKHR: {}", reinterpret_cast<void*>(fnAcquireNextImageKHR));
        LOG_INFO("Vulkan: fnAcquireNextImage2KHR: {}", reinterpret_cast<void*>(fnAcquireNextImage2KHR));
        LOG_INFO("Vulkan: fnQueuePresentKHR: {}", reinterpret_cast<void*>(fnQueuePresentKHR));
        LOG_INFO("Vulkan: fnCreateSwapchainKHR: {}", reinterpret_cast<void*>(fnCreateSwapchainKHR));

        bool ok = true;
        ok &= backend::CreateHookOnce(fnAcquireNextImageKHR, &Hook_AcquireNextImageKHR,
                                      reinterpret_cast<void**>(&oAcquireNextImageKHR), "vkAcquireNextImageKHR") != nullptr;
        ok &= backend::CreateHookOnce(fnAcquireNextImage2KHR, &Hook_AcquireNextImage2KHR,
                                      reinterpret_cast<void**>(&oAcquireNextImage2KHR),
                                      "vkAcquireNextImage2KHR") != nullptr;
        ok &= backend::CreateHookOnce(fnQueuePresentKHR, &Hook_QueuePresentKHR,
                                      reinterpret_cast<void**>(&oQueuePresentKHR), "vkQueuePresentKHR") != nullptr;
        ok &= backend::CreateHookOnce(fnCreateSwapchainKHR, &Hook_CreateSwapchainKHR,
                                      reinterpret_cast<void**>(&oCreateSwapchainKHR), "vkCreateSwapchainKHR") != nullptr;

        backend::EnableHook(fnAcquireNextImageKHR, "vkAcquireNextImageKHR");
        backend::EnableHook(fnAcquireNextImage2KHR, "vkAcquireNextImage2KHR");
        backend::EnableHook(fnQueuePresentKHR, "vkQueuePresentKHR");
        backend::EnableHook(fnCreateSwapchainKHR, "vkCreateSwapchainKHR");
    }

    return true;
}

void VULKAN_Backend::Shutdown()
{
    if (ImGui::GetCurrentContext())
    {
        if (ImGui::GetIO().BackendRendererUserData)
            ImGui_ImplVulkan_Shutdown();

        if (ImGui::GetIO().BackendPlatformUserData)
            ImGui_ImplWin32_Shutdown();

        ImGui::DestroyContext();
    }

    CleanupDeviceVulkan();
}
