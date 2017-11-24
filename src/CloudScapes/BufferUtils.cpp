#include "BufferUtils.h"

void BufferUtils::CreateBuffer(VulkanDevice* device, VkBufferUsageFlags allowedUsage, uint32_t size,
								VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
	// Create buffer
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = size;	// Size in bytes
	bufferCreateInfo.usage = allowedUsage;	// Which purpose the data in the buffer will be used 
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; //VK_SHARING_MODE_EXCLUSIVE specifies that access to any range or 
															  //image subresource of the object will be exclusive to a single queue family at a time.

	if (vkCreateBuffer(device->GetVkDevice(), &bufferCreateInfo, nullptr, &buffer) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create buffer");
	}

	// Query buffer's memory requirements
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device->GetVkDevice(), buffer, &memRequirements);

	// Allocate memory for the buffer in the device
	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = device->GetInstance()->GetMemoryTypeIndex(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(device->GetVkDevice(), &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate device memory for the buffer");
	}

	// Associate allocated memory with vertex buffer
	vkBindBufferMemory(device->GetVkDevice(), buffer, bufferMemory, 0);
}

void BufferUtils::CreateBufferFromData(VulkanDevice* device, VkCommandPool commandPool, void* bufferData, VkDeviceSize bufferSize,
									VkBufferUsageFlags bufferUsage, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
	// Create the staging buffer
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	VkBufferUsageFlags stagingUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	VkMemoryPropertyFlags stagingProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	BufferUtils::CreateBuffer(device, bufferSize, stagingUsage, stagingProperties, stagingBuffer, stagingBufferMemory);

	// Fill the staging buffer
	void *data;
	vkMapMemory(device->GetVkDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, bufferData, static_cast<size_t>(bufferSize));
	vkUnmapMemory(device->GetVkDevice(), stagingBufferMemory);

	// Create the buffer
	VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | bufferUsage;
	VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	BufferUtils::CreateBuffer(device, bufferSize, usage, flags, buffer, bufferMemory);

	// Copy data from staging buffer to the actual buffer
	BufferUtils::CopyBuffer(device, commandPool, stagingBuffer, buffer, bufferSize);

	// Destroy the staging buffer and free its memory
	vkDestroyBuffer(device->GetVkDevice(), stagingBuffer, nullptr);
	vkFreeMemory(device->GetVkDevice(), stagingBufferMemory, nullptr);
}

//Copy data from Source Buffer to Destination Buffer
void BufferUtils::CopyBuffer(VulkanDevice* device, VkCommandPool commandPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) 
{
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(device->GetVkDevice(), &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	VkBufferCopy copyRegion = {};
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(device->GetQueue(QueueFlags::Graphics), 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(device->GetQueue(QueueFlags::Graphics));
	vkFreeCommandBuffers(device->GetVkDevice(), commandPool, 1, &commandBuffer);
}

// Allocates device memory of a certain size
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