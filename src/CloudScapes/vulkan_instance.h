#pragma once

#include <bitset>
#include <vector>
#include <vulkan/vulkan.h>
#include "forward.h"
#include "vulkan_device.h"

extern const bool ENABLE_VALIDATION;

class VulkanInstance {

public:
    VulkanInstance() = delete;
    VulkanInstance(const char* applicationName, unsigned int additionalExtensionCount = 0, const char** additionalExtensions = nullptr);

    VkInstance GetVkInstance();
    VkPhysicalDevice GetPhysicalDevice();
    const QueueFamilyIndices& GetQueueFamilyIndices() const;
    const VkSurfaceCapabilitiesKHR& GetSurfaceCapabilities() const;
    const std::vector<VkSurfaceFormatKHR>& GetSurfaceFormats() const;
    const std::vector<VkPresentModeKHR>& GetPresentModes() const;
    
    uint32_t GetMemoryTypeIndex(uint32_t types, VkMemoryPropertyFlags properties) const;

    void PickPhysicalDevice(std::vector<const char*> deviceExtensions, QueueFlagBits requiredQueues, VkSurfaceKHR surface = VK_NULL_HANDLE);

    VulkanDevice* CreateDevice(QueueFlagBits requiredQueues);

    ~VulkanInstance();

private:

    void initDebugReport();

    VkInstance instance;
    VkDebugReportCallbackEXT debugReportCallback;
    std::vector<const char*> deviceExtensions;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    QueueFamilyIndices queueFamilyIndices;
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    std::vector<VkSurfaceFormatKHR> surfaceFormats;
    std::vector<VkPresentModeKHR> presentModes;
    VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
};