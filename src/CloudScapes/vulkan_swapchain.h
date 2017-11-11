#pragma once
#include "forward.h"
#include "vulkan_device.h"

class VulkanSwapChain {
    friend class VulkanDevice;

public:
    VkSwapchainKHR GetVulkanSwapChain() const;
    VkFormat GetImageFormat() const;
    VkExtent2D GetExtent() const;
    uint32_t GetIndex() const;
    uint32_t GetCount() const;
    VkImageView GetImageView(uint32_t index) const;
    VkSemaphore GetImageAvailableSemaphore() const;
    VkSemaphore GetRenderFinishedSemaphore() const;

    void Acquire();
    void Present();
    ~VulkanSwapChain();

private:
    VulkanSwapChain(VulkanDevice* device, VkSurfaceKHR vkSurface);

    VulkanDevice* device;
    VkSurfaceKHR vkSurface;
    VkSwapchainKHR vkSwapChain;
    std::vector<VkImage> vkSwapChainImages;
    std::vector<VkImageView> vkSwapChainImageViews;
    VkFormat vkSwapChainImageFormat;
    VkExtent2D vkSwapChainExtent;
    uint32_t imageIndex = 0;

    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
};