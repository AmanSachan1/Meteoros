#pragma once

#include "VMA_Utility.h"
#define VMA_IMPLEMENTATION
#include "../../external/vk_mem_alloc.h"

void VMA_Utility::createBufferVMA(VmaAllocator& g_vma_Allocator, VkBufferUsageFlags allowedUsage, uint32_t size,
	VkBuffer& buffer, VmaAllocation& g_vma_BufferAlloc)
{
	VkBufferCreateInfo vbInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	vbInfo.size = size;
	vbInfo.usage = allowedUsage;
	vbInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo vbAllocCreateInfo = {};
	vbAllocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	vbAllocCreateInfo.flags = 0;

	ERR_GUARD_VULKAN(vmaCreateBuffer(g_vma_Allocator, &vbInfo, &vbAllocCreateInfo, &buffer, &g_vma_BufferAlloc, nullptr));
}

void VMA_Utility::createVertexandIndexBuffersVMA(VulkanDevice* device, VkCommandPool commandPool, VmaAllocator& g_vma_Allocator,
	const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices,
	VkBuffer& vertexBuffer, VkBuffer& indexBuffer,
	VmaAllocation& g_vma_VertexBufferAlloc, VmaAllocation& g_vma_IndexBufferAlloc)
{
	// Vertex Buffer
	VkBufferCreateInfo vbInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	vbInfo.size = vertices.size() * sizeof(vertices[0]);
	vbInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	vbInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo vbAllocCreateInfo = {};
	vbAllocCreateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
	vbAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

	VkBuffer stagingVertexBuffer = VK_NULL_HANDLE;
	VmaAllocation stagingVertexBufferAlloc = VK_NULL_HANDLE;
	VmaAllocationInfo stagingVertexBufferAllocInfo = {};
	ERR_GUARD_VULKAN(vmaCreateBuffer(g_vma_Allocator, &vbInfo, &vbAllocCreateInfo, &stagingVertexBuffer, &stagingVertexBufferAlloc, &stagingVertexBufferAllocInfo));

	memcpy(stagingVertexBufferAllocInfo.pMappedData, vertices.data(), vbInfo.size);

	// No need to flush stagingVertexBuffer memory because CPU_ONLY memory is always HOST_COHERENT.

	vbInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	vbAllocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	vbAllocCreateInfo.flags = 0;
	ERR_GUARD_VULKAN(vmaCreateBuffer(g_vma_Allocator, &vbInfo, &vbAllocCreateInfo, &vertexBuffer, &g_vma_VertexBufferAlloc, nullptr));

	// Index Buffer
	VkBufferCreateInfo ibInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	ibInfo.size = indices.size() * sizeof(indices[0]);
	ibInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	ibInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo ibAllocCreateInfo = {};
	ibAllocCreateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
	ibAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

	VkBuffer stagingIndexBuffer = VK_NULL_HANDLE;
	VmaAllocation stagingIndexBufferAlloc = VK_NULL_HANDLE;
	VmaAllocationInfo stagingIndexBufferAllocInfo = {};
	ERR_GUARD_VULKAN(vmaCreateBuffer(g_vma_Allocator, &ibInfo, &ibAllocCreateInfo, &stagingIndexBuffer, &stagingIndexBufferAlloc, &stagingIndexBufferAllocInfo));

	memcpy(stagingIndexBufferAllocInfo.pMappedData, indices.data(), ibInfo.size);

	// No need to flush stagingIndexBuffer memory because CPU_ONLY memory is always HOST_COHERENT.

	ibInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	ibAllocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	ibAllocCreateInfo.flags = 0;
	ERR_GUARD_VULKAN(vmaCreateBuffer(g_vma_Allocator, &ibInfo, &ibAllocCreateInfo, &indexBuffer, &g_vma_IndexBufferAlloc, nullptr));

	// Copy buffers
	VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

	VkBufferCopy vbCopyRegion = {};
	vbCopyRegion.srcOffset = 0;
	vbCopyRegion.dstOffset = 0;
	vbCopyRegion.size = vbInfo.size;
	vkCmdCopyBuffer(commandBuffer, stagingVertexBuffer, vertexBuffer, 1, &vbCopyRegion);

	VkBufferCopy ibCopyRegion = {};
	ibCopyRegion.srcOffset = 0;
	ibCopyRegion.dstOffset = 0;
	ibCopyRegion.size = ibInfo.size;
	vkCmdCopyBuffer(commandBuffer, stagingIndexBuffer, indexBuffer, 1, &ibCopyRegion);

	endSingleTimeCommands(device, commandPool, commandBuffer);

	vmaDestroyBuffer(g_vma_Allocator, stagingIndexBuffer, stagingIndexBufferAlloc);
	vmaDestroyBuffer(g_vma_Allocator, stagingVertexBuffer, stagingVertexBufferAlloc);
}

void VMA_Utility::createBufferFromDataVMA(VulkanDevice* device, VkCommandPool commandPool, VmaAllocator& g_vma_Allocator,
	void* bufferData, VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage,
	VkBuffer& buffer, VmaAllocation& g_vma_BufferAlloc)
{
	// Staging Buffer
	VkBufferCreateInfo stagingBufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	stagingBufferInfo.size = bufferSize;
	stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo stagingBufferAllocCreateInfo = {};
	stagingBufferAllocCreateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
	stagingBufferAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

	VkBuffer stagingBuffer = VK_NULL_HANDLE;
	VmaAllocation stagingBufferAlloc = VK_NULL_HANDLE;
	VmaAllocationInfo stagingBufferAllocInfo = {};
	ERR_GUARD_VULKAN(vmaCreateBuffer(g_vma_Allocator, &stagingBufferInfo, &stagingBufferAllocCreateInfo,
		&stagingBuffer, &stagingBufferAlloc, &stagingBufferAllocInfo));

	//copy data over to staging buffer
	memcpy(stagingBufferAllocInfo.pMappedData, bufferData, bufferSize);

	// No need to flush stagingBuffer memory because CPU_ONLY memory is always HOST_COHERENT.

	//create actual buffer
	VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferInfo.size = bufferSize;
	bufferInfo.usage = bufferUsage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo bufferAllocCreateInfo = {};
	bufferAllocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	bufferAllocCreateInfo.flags = 0;
	ERR_GUARD_VULKAN(vmaCreateBuffer(g_vma_Allocator, &bufferInfo, &bufferAllocCreateInfo, &buffer, &g_vma_BufferAlloc, nullptr));

	// Copy data of staging buffer into actual buffer that was passed in
	VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

	VkBufferCopy copyRegion = {};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = bufferSize;
	vkCmdCopyBuffer(commandBuffer, stagingBuffer, buffer, 1, &copyRegion);

	endSingleTimeCommands(device, commandPool, commandBuffer);

	vmaDestroyBuffer(g_vma_Allocator, stagingBuffer, stagingBufferAlloc);
}

void VMA_Utility::createImageVMA(VmaAllocator& g_vma_Allocator, VkImageCreateInfo& imageInfo, VmaAllocationCreateInfo& imageAllocCreateInfo, 
								VkImage& image, VmaAllocation& ImageAlloc, VmaAllocationInfo allocInfo)
{
	ERR_GUARD_VULKAN(vmaCreateImage(g_vma_Allocator, &imageInfo, &imageAllocCreateInfo, &image, &ImageAlloc, &allocInfo));
}

VkBuffer VMA_Utility::createStagingBuffer(VulkanDevice* device, VkCommandPool& commandPool, VmaAllocator& g_vma_Allocator,
										void* bufferData, VmaAllocation& stagingBufferAlloc, VkDeviceSize bufferSize)
{
	//----------------------
	//--- Staging Buffer ---
	//----------------------
	VkBufferCreateInfo stagingBufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	stagingBufferInfo.size = bufferSize;
	stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo stagingBufferAllocCreateInfo = {};
	stagingBufferAllocCreateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
	stagingBufferAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

	VkBuffer stagingBuffer = VK_NULL_HANDLE;
	VmaAllocationInfo stagingBufferAllocInfo = {};
	ERR_GUARD_VULKAN(vmaCreateBuffer(g_vma_Allocator, &stagingBufferInfo, &stagingBufferAllocCreateInfo,
									 &stagingBuffer, &stagingBufferAlloc, &stagingBufferAllocInfo));

	//copy data over to staging buffer
	memcpy(stagingBufferAllocInfo.pMappedData, bufferData, bufferSize);

	return stagingBuffer;
}

void VMA_Utility::destroyStagingBuffer(VmaAllocator& g_vma_Allocator, VkBuffer& stagingBuffer, VmaAllocation& stagingBufferAlloc)
{
	vmaDestroyBuffer(g_vma_Allocator, stagingBuffer, stagingBufferAlloc);
}

void VMA_Utility::create3DTexturefromData(VulkanDevice* device, VkDevice& logicalDevice, VkCommandPool& commandPool, VmaAllocator& g_vma_Allocator,
	VkDeviceSize imageSize, uint32_t width, uint32_t height, uint32_t depth, void* textureData,
	VkImage &textureImage, VmaAllocation& textureImageAlloc)
{
	//VkImageCreateInfo stagingImageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	//stagingImageInfo.imageType = VK_IMAGE_TYPE_3D;
	//stagingImageInfo.extent.width = width;
	//stagingImageInfo.extent.height = height;
	//stagingImageInfo.extent.depth = depth;
	//stagingImageInfo.mipLevels = 1;
	//stagingImageInfo.arrayLayers = 1;
	//stagingImageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	//stagingImageInfo.tiling = VK_IMAGE_TILING_LINEAR;
	//stagingImageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
	//stagingImageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	//stagingImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	//stagingImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	//stagingImageInfo.flags = 0;

	//VmaAllocationCreateInfo stagingImageAllocCreateInfo = {};
	//stagingImageAllocCreateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
	//stagingImageAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

	//VkImage stagingImage = VK_NULL_HANDLE;
	//VmaAllocation stagingImageAlloc = VK_NULL_HANDLE;
	//VmaAllocationInfo stagingImageAllocInfo = {};
	//ERR_GUARD_VULKAN(vmaCreateImage(g_vma_Allocator, &stagingImageInfo, &stagingImageAllocCreateInfo, &stagingImage, &stagingImageAlloc, &stagingImageAllocInfo));

	//VkImageSubresource imageSubresource = {};
	//imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//imageSubresource.mipLevel = 0;
	//imageSubresource.arrayLayer = 0;

	//VkSubresourceLayout imageLayout = {};
	//vkGetImageSubresourceLayout(logicalDevice, stagingImage, &imageSubresource, &imageLayout);

	//stagingImageAllocInfo.pMappedData = textureData;
	//// No need to flush stagingImage memory because CPU_ONLY memory is always HOST_COHERENT.

	//// Create textureImage in GPU memory.

	//VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	//imageInfo.imageType = VK_IMAGE_TYPE_2D;
	//imageInfo.extent.width = width;
	//imageInfo.extent.height = height;
	//imageInfo.extent.depth = depth;
	//imageInfo.mipLevels = 1;
	//imageInfo.arrayLayers = 1;
	//imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	//imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	//imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	//imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	//imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	//imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	//imageInfo.flags = 0;

	//VmaAllocationCreateInfo imageAllocCreateInfo = {};
	//imageAllocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	//ERR_GUARD_VULKAN(vmaCreateImage(g_vma_Allocator, &imageInfo, &imageAllocCreateInfo, &textureImage, &textureImageAlloc, nullptr));

	//// Transition image layouts, copy image.

	//VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

	//VkImageMemoryBarrier imgMemBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	//imgMemBarrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
	//imgMemBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	//imgMemBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	//imgMemBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	//imgMemBarrier.image = stagingImage;
	//imgMemBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//imgMemBarrier.subresourceRange.baseMipLevel = 0;
	//imgMemBarrier.subresourceRange.levelCount = 1;
	//imgMemBarrier.subresourceRange.baseArrayLayer = 0;
	//imgMemBarrier.subresourceRange.layerCount = 1;
	//imgMemBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
	//imgMemBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

	//vkCmdPipelineBarrier(
	//	commandBuffer,
	//	VK_PIPELINE_STAGE_HOST_BIT,
	//	VK_PIPELINE_STAGE_TRANSFER_BIT,
	//	0,
	//	0, nullptr,
	//	0, nullptr,
	//	1, &imgMemBarrier);

	//imgMemBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	//imgMemBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	//imgMemBarrier.image = textureImage;
	//imgMemBarrier.srcAccessMask = 0;
	//imgMemBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

	//vkCmdPipelineBarrier(
	//	commandBuffer,
	//	VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
	//	VK_PIPELINE_STAGE_TRANSFER_BIT,
	//	0,
	//	0, nullptr,
	//	0, nullptr,
	//	1, &imgMemBarrier);

	//VkImageCopy imageCopy = {};
	//imageCopy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//imageCopy.srcSubresource.baseArrayLayer = 0;
	//imageCopy.srcSubresource.mipLevel = 0;
	//imageCopy.srcSubresource.layerCount = 1;
	//imageCopy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//imageCopy.dstSubresource.baseArrayLayer = 0;
	//imageCopy.dstSubresource.mipLevel = 0;
	//imageCopy.dstSubresource.layerCount = 1;
	//imageCopy.srcOffset.x = 0;
	//imageCopy.srcOffset.y = 0;
	//imageCopy.srcOffset.z = 0;
	//imageCopy.dstOffset.x = 0;
	//imageCopy.dstOffset.y = 0;
	//imageCopy.dstOffset.z = 0;
	//imageCopy.extent.width = width;
	//imageCopy.extent.height = height;
	//imageCopy.extent.depth = depth;
	//vkCmdCopyImage(
	//	commandBuffer,
	//	stagingImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
	//	textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	//	1, &imageCopy);

	//imgMemBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	//imgMemBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	//imgMemBarrier.image = textureImage;
	//imgMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	//imgMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	//vkCmdPipelineBarrier(
	//	commandBuffer,
	//	VK_PIPELINE_STAGE_TRANSFER_BIT,
	//	VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
	//	0,
	//	0, nullptr,
	//	0, nullptr,
	//	1, &imgMemBarrier);

	//endSingleTimeCommands(device, commandPool, commandBuffer);

	//vmaDestroyImage(g_vma_Allocator, stagingImage, stagingImageAlloc);
}
