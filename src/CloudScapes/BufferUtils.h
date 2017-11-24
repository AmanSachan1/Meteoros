#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "VulkanDevice.h"

namespace BufferUtils 
{
	void CreateBuffer(VulkanDevice* device, VkBufferUsageFlags allowedUsage, uint32_t size,
					VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

	void CreateBufferFromData(VulkanDevice* device, VkCommandPool commandPool, void* bufferData, VkDeviceSize bufferSize, 
							  VkBufferUsageFlags bufferUsage, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

	void CopyBuffer(VulkanDevice* device, VkCommandPool commandPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

	VkDeviceMemory CreateDeviceMemory(VulkanDevice* device, uint32_t size, uint32_t types, VkMemoryPropertyFlags propertyFlags);
}