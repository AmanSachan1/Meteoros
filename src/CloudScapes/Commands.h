#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "VulkanDevice.h"

VkCommandBuffer beginSingleTimeCommands(VulkanDevice* device, VkCommandPool commandPool);
void endSingleTimeCommands(VulkanDevice* device, VkCommandPool commandPool, VkCommandBuffer commandBuffer);