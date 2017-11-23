#include "BufferUtils.h"

// Allocate device memory for buffer
VkDeviceMemory BufferUtils::CreateDeviceMemory(VulkanDevice* device, uint32_t size, uint32_t types, VkMemoryPropertyFlags propertyFlags)
{
	VkMemoryAllocateInfo memoryAllocateInfo = {};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.allocationSize = size;
	memoryAllocateInfo.memoryTypeIndex = device->GetInstance()->GetMemoryTypeIndex(types, propertyFlags);
	
	VkDeviceMemory deviceMemory;
	if (vkAllocateMemory(device->GetVkDevice(), &memoryAllocateInfo, nullptr, &deviceMemory) != VK_SUCCESS) 
	{
		throw std::runtime_error("failed to allocate device memory!");
	}
	
	return deviceMemory;
}

// Assign memory to buffers created in CreateBuffer
VkDeviceMemory BufferUtils::AllocateMemoryForBuffers(VulkanDevice* device, std::vector<VkBuffer> buffers,
													 VkMemoryPropertyFlags propertyFlags, unsigned int* bufferOffsets)
{
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
		vkGetBufferMemoryRequirements(device->GetVkDevice(), buffers[i], &memoryRequirements);
		typeBits |= memoryRequirements.memoryTypeBits;
	}// end for loop

	return CreateDeviceMemory(device, bufferOffsets[buffers.size() - 1] + memoryRequirements.size, typeBits, propertyFlags);
}

// Actually create the buffer
VkBuffer BufferUtils::CreateBuffer(VulkanDevice* device, VkBufferUsageFlags allowedUsage, uint32_t size,
								   VkDeviceMemory deviceMemory, uint32_t deviceMemoryOffset)
{
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = size;	// Size in bytes
	bufferCreateInfo.usage = allowedUsage;	// Which purpose the data in the buffer will be used 
	
	VkBuffer buffer;
	vkCreateBuffer(device->GetVkDevice(), &bufferCreateInfo, nullptr, &buffer);

	return buffer;
}

/*
	Binds memory to buffer
	Note: "buffer" is a handle
*/
void BufferUtils::BindMemoryForBuffers(VulkanDevice* device, std::vector<VkBuffer> buffers, VkDeviceMemory bufferMemory, unsigned int* bufferOffsets)
{
	for (unsigned int i = 0; i < buffers.size(); ++i) 
	{
		vkBindBufferMemory(device->GetVkDevice(), buffers[i], bufferMemory, bufferOffsets[i]);
	}
}