#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "vulkan_device.h"

VkDeviceMemory CreateDeviceMemory(VulkanDevice* device, 
									uint32_t size, 
									uint32_t types, 
									VkMemoryPropertyFlags propertyFlags);

VkBuffer CreateBuffer(VulkanDevice* device, 
						VkBufferUsageFlags allowedUsage, 
						uint32_t size, 
						VkDeviceMemory deviceMemory = VK_NULL_HANDLE, 
						uint32_t deviceMemoryOffset = 0);

VkDeviceMemory AllocateMemoryForBuffers(VulkanDevice* device, 
										std::vector<VkBuffer> buffers, 
										VkMemoryPropertyFlags propertyFlags, 
										unsigned int* bufferOffsets);

void BindMemoryForBuffers(VulkanDevice* device, 
							VkDeviceMemory bufferMemory,
							std::vector<VkBuffer> buffers, 
							unsigned int* bufferOffsets);