#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "VulkanDevice.h"

namespace BufferUtils 
{
	VkDeviceMemory CreateDeviceMemory(VulkanDevice* device, uint32_t size, uint32_t types, VkMemoryPropertyFlags propertyFlags);

	VkDeviceMemory AllocateMemoryForBuffers(VulkanDevice* device, std::vector<VkBuffer> buffers, 
											VkMemoryPropertyFlags propertyFlags, unsigned int* bufferOffsets);

	VkBuffer CreateBuffer(VulkanDevice* device, VkBufferUsageFlags allowedUsage, uint32_t size,
						  VkDeviceMemory deviceMemory = VK_NULL_HANDLE, uint32_t deviceMemoryOffset = 0);

	void BindMemoryForBuffers(VulkanDevice* device, std::vector<VkBuffer> buffers, VkDeviceMemory bufferMemory, unsigned int* bufferOffsets);
}