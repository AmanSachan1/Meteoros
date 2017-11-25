#include "Commands.h"

//Reference: https://vulkan-tutorial.com/Texture_mapping/Images
VkCommandBuffer beginSingleTimeCommands(VulkanDevice* device, VkCommandPool commandPool)
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

	return commandBuffer;
}

// This function flushes the command buffer; that is it submits the command to the queue and then 
// free's memory associated with the command buffer(thereby flushing it)
void endSingleTimeCommands(VulkanDevice* device, VkCommandPool commandPool, VkCommandBuffer commandBuffer)
{
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(device->GetQueue(QueueFlags::Graphics), 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(device->GetQueue(QueueFlags::Graphics));

	vkFreeCommandBuffers(device->GetVkDevice(), commandPool, 1, &commandBuffer);
}

void endSingleTimeCommands(VulkanDevice* device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer)
{
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(queue);

	vkFreeCommandBuffers(device->GetVkDevice(), commandPool, 1, &commandBuffer);
}