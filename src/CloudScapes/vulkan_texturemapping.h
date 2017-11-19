#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "vulkan_device.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../../external/ImageLibraries/stb_image.h"

VkCommandBuffer beginSingleTimeCommands(VulkanDevice* device, VkCommandPool commandPool);
void endSingleTimeCommands(VulkanDevice* device, VkCommandPool commandPool, VkCommandBuffer commandBuffer);

void copyBuffer(VulkanDevice* device, VkCommandPool commandPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
void transitionImageLayout(VulkanDevice* device, VkCommandPool commandPool, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
void copyBufferToImage(VulkanDevice* device, VkCommandPool commandPool, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

//load an image and upload it into a Vulkan image object
void createTextureImage(VulkanDevice* device);
void createImage(VulkanDevice* device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, 
				VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);