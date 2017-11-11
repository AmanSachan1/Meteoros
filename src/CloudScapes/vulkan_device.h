#pragma once
#include <array>
#include <vulkan/vulkan.h>
#include "forward.h"
#include "vulkan_instance.h"
#include "vulkan_swapchain.h"

class VulkanDevice {
    friend class VulkanInstance;

public:
    VulkanSwapChain* CreateSwapChain(VkSurfaceKHR surface);
    VulkanInstance* GetInstance();
    VkDevice GetVulkanDevice();
    VkQueue GetQueue(QueueFlags flag);
    ~VulkanDevice();

private:
    using Queues = std::array<VkQueue, sizeof(QueueFlags)>;
    
    VulkanDevice() = delete;
    VulkanDevice(VulkanInstance* instance, VkDevice vkDevice, Queues queues);

    VulkanInstance* instance;
    VkDevice vkDevice;
    Queues queues;
    QueueFamilyIndices queueIndices;
};