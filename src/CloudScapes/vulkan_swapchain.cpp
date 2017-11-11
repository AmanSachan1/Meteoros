#include <vector>
#include "forward.h"
#include "vulkan_swapchain.h"
#include "vulkan_instance.h"
#include "vulkan_device.h"
#include "window.h"

namespace {
  // Specify the color channel format and color space type
  VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
      // VK_FORMAT_UNDEFINED indicates that the surface has no preferred format, so we can choose any
      if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) {
          return{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
      }

      // Otherwise, choose a preferred combination
      for (const auto& availableFormat : availableFormats) {
          // Ideal format and color space
          if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
              return availableFormat;
          }
      }

      // Otherwise, return any format
      return availableFormats[0];
  }

  // Specify the presentation mode of the swap chain
  VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes) {
      // Second choice
      VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;
      
      for (const auto& availablePresentMode : availablePresentModes) {
          if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
              // First choice
              return availablePresentMode;
          }
          else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
              // Third choice
              bestMode = availablePresentMode;
          }
      }

      return bestMode;
  }

  // Specify the swap extent (resolution) of the swap chain
  VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window) {
      if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
          return capabilities.currentExtent;
      }
      else {
          int width, height;
          glfwGetWindowSize(window, &width, &height);
          VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

          actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
          actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

          return actualExtent;
      }
  }
}

VulkanSwapChain::VulkanSwapChain(VulkanDevice* device, VkSurfaceKHR vkSurface)
  : device(device), vkSurface(vkSurface) {
    
    auto* instance = device->GetInstance();
    
    const auto& surfaceCapabilities = instance->GetSurfaceCapabilities();

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(instance->GetSurfaceFormats());
    VkPresentModeKHR presentMode = chooseSwapPresentMode(instance->GetPresentModes());
    VkExtent2D extent = chooseSwapExtent(surfaceCapabilities, GetGLFWWindow());

    uint32_t imageCount = surfaceCapabilities.minImageCount + 1; // 2 + 1 = triple buffering
    if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount) {
        imageCount = surfaceCapabilities.maxImageCount;
    }

    // --- Create swap chain ---
    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;

    // Specify surface to be tied to
    createInfo.surface = vkSurface;

    // Add details of the swap chain
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const auto& queueFamilyIndices = instance->GetQueueFamilyIndices();
    if (queueFamilyIndices[QueueFlags::Graphics] != queueFamilyIndices[QueueFlags::Present]) {
        // Images can be used across multiple queue families without explicit ownership transfers
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        unsigned int indices[] = { queueFamilyIndices[QueueFlags::Graphics], queueFamilyIndices[QueueFlags::Present] };
        createInfo.pQueueFamilyIndices = indices;
    } else {
        // An image is owned by one queue family at a time and ownership must be explicitly transfered between uses
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    // Specify transform on images in the swap chain (no transformation done here)
    createInfo.preTransform = surfaceCapabilities.currentTransform;
  
    // Specify alpha channel usage (set to be ignored here)
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  
    // Specify presentation mode
    createInfo.presentMode = presentMode;
  
    // Specify whether we can clip pixels that are obscured by other windows
    createInfo.clipped = VK_TRUE;
  
    // Reference to old swap chain in case current one becomes invalid
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    // Create swap chain
    if (vkCreateSwapchainKHR(device->GetVulkanDevice(), &createInfo, nullptr, &vkSwapChain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swap chain");
    }

    // --- Retrieve swap chain images ---
    vkGetSwapchainImagesKHR(device->GetVulkanDevice(), vkSwapChain, &imageCount, nullptr);
    vkSwapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device->GetVulkanDevice(), vkSwapChain, &imageCount, vkSwapChainImages.data());

    vkSwapChainImageFormat = surfaceFormat.format;
    vkSwapChainExtent = extent;

    vkSwapChainImageViews.resize(vkSwapChainImages.size());

    for (size_t i = 0; i < vkSwapChainImages.size(); i++) {
        // --- Create an image view for each swap chain image ---
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = vkSwapChainImages[i];

        // Specify how the image data should be interpreted
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = vkSwapChainImageFormat;

        // Specify color channel mappings (can be used for swizzling)
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        // Describe the image's purpose and which part of the image should be accessed
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        // Create the image view
        if (vkCreateImageView(device->GetVulkanDevice(), &createInfo, nullptr, &vkSwapChainImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image views");
        }
    }

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    if (vkCreateSemaphore(device->GetVulkanDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
        vkCreateSemaphore(device->GetVulkanDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create semaphores");
    }
}

VkSwapchainKHR VulkanSwapChain::GetVulkanSwapChain() const {
    return vkSwapChain;
}

VkFormat VulkanSwapChain::GetImageFormat() const {
    return vkSwapChainImageFormat;
}

VkExtent2D VulkanSwapChain::GetExtent() const {
    return vkSwapChainExtent;
}

uint32_t VulkanSwapChain::GetIndex() const {
    return imageIndex;
}

uint32_t VulkanSwapChain::GetCount() const {
    return static_cast<uint32_t>(vkSwapChainImages.size());
}

VkImageView VulkanSwapChain::GetImageView(uint32_t index) const {
    return vkSwapChainImageViews[index];
}

VkSemaphore VulkanSwapChain::GetImageAvailableSemaphore() const {
    return imageAvailableSemaphore;

}

VkSemaphore VulkanSwapChain::GetRenderFinishedSemaphore() const {
    return renderFinishedSemaphore;
}

void VulkanSwapChain::Acquire() {
    if (ENABLE_VALIDATION) {
        // the validation layer implementation expects the application to explicitly synchronize with the GPU
        vkQueueWaitIdle(device->GetQueue(QueueFlags::Present));
    }
    VkResult result = vkAcquireNextImageKHR(device->GetVulkanDevice(), vkSwapChain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);   
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swap chain image");
    }
}

void VulkanSwapChain::Present() {
    VkSemaphore signalSemaphores[] = { renderFinishedSemaphore };

    // Submit result back to swap chain for presentation
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { vkSwapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;

    VkResult result = vkQueuePresentKHR(device->GetQueue(QueueFlags::Present), &presentInfo);

    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swap chain image");
    }
}

VulkanSwapChain::~VulkanSwapChain() {
    vkDestroySemaphore(device->GetVulkanDevice(), imageAvailableSemaphore, nullptr);
    vkDestroySemaphore(device->GetVulkanDevice(), renderFinishedSemaphore, nullptr);

    for (size_t i = 0; i < vkSwapChainImageViews.size(); i++) {
        vkDestroyImageView(device->GetVulkanDevice(), vkSwapChainImageViews[i], nullptr);
    }

    vkDestroySwapchainKHR(device->GetVulkanDevice(), vkSwapChain, nullptr);
}