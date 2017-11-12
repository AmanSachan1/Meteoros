#include "vulkan_buffer.h"

VkDeviceMemory CreateDeviceMemory(VulkanDevice* device,
									uint32_t size,
									uint32_t types,
									VkMemoryPropertyFlags propertyFlags)
{
	VkMemoryAllocateInfo memoryAllocateInfo = {};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.allocationSize = size;
	memoryAllocateInfo.memoryTypeIndex = device->GetInstance()->GetMemoryTypeIndex(types, propertyFlags);
	
	VkDeviceMemory deviceMemory;
	vkAllocateMemory(device->GetVulkanDevice(), &memoryAllocateInfo, nullptr, &deviceMemory);
	return deviceMemory;
}

/*
	Create buffers (called in main.cpp)
*/
VkBuffer CreateBuffer(VulkanDevice* device,
						VkBufferUsageFlags allowedUsage,
						uint32_t size,
						VkDeviceMemory deviceMemory,
						uint32_t deviceMemoryOffset)
{
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = size;	// Size in bytes
	bufferCreateInfo.usage = allowedUsage;	// Which purpose the data in the buffer will be used 
	
	VkBuffer buffer;
	vkCreateBuffer(device->GetVulkanDevice(), &bufferCreateInfo, nullptr, &buffer);

	if (deviceMemory != VK_NULL_HANDLE) 
	{
		vkBindBufferMemory(device->GetVulkanDevice(), buffer, deviceMemory, deviceMemoryOffset);
	}

	return buffer;
}

/*
	Assign memory to vertex buffers created in CreateBuffer (called in main.cpp)
*/
VkDeviceMemory AllocateMemoryForBuffers(VulkanDevice* device,
										std::vector<VkBuffer> buffers,
										VkMemoryPropertyFlags propertyFlags,
										unsigned int* bufferOffsets)
{
	// We will allocate one large chunk of memory for both vertices and indices

	VkMemoryRequirements memoryRequirements;
	uint32_t typeBits = 0;
	for (unsigned int i = 0; i < buffers.size(); ++i) 
	{
		if (i == 0) 
		{
			bufferOffsets[i] = 0;
		}

		else 
		{
			bufferOffsets[i] = bufferOffsets[i - 1] + memoryRequirements.size;
		}
		
		// Query its memory requirements (aka figure out what kind of memory we need and how much)
		vkGetBufferMemoryRequirements(device->GetVulkanDevice(), buffers[i], &memoryRequirements);
		typeBits |= memoryRequirements.memoryTypeBits;
	}// end for loop

	return CreateDeviceMemory(device, bufferOffsets[buffers.size() - 1] + memoryRequirements.size, typeBits, propertyFlags);
}

/*
	Binds memory to buffer (called in main.cpp)
	Note: "buffer" is a handle
*/
void BindMemoryForBuffers(VulkanDevice* device,
							VkDeviceMemory bufferMemory,
							std::vector<VkBuffer> buffers,
							unsigned int* bufferOffsets)
{
	for (unsigned int i = 0; i < buffers.size(); ++i) 
	{
		vkBindBufferMemory(device->GetVulkanDevice(), buffers[i], bufferMemory, bufferOffsets[i]);
	}
}