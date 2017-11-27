#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "VulkanDevice.h"
#include "Commands.h"
#include "Vertex.h"
#include "../../external/vk_mem_alloc.h"

#define ERR_GUARD_VULKAN(Expr) do { VkResult res__ = (Expr); if (res__ < 0) assert(0); } while(0)

namespace BufferUtils 
{
	void CreateBuffer(VulkanDevice* device, VkBufferUsageFlags allowedUsage, uint32_t size,
					VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

	void CreateBufferFromData(VulkanDevice* device, VkCommandPool commandPool, void* bufferData, VkDeviceSize bufferSize, 
							  VkBufferUsageFlags bufferUsage, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

	void CopyBuffer(VulkanDevice* device, VkCommandPool commandPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

	VkDeviceMemory CreateDeviceMemory(VulkanDevice* device, uint32_t size, uint32_t types, VkMemoryPropertyFlags propertyFlags);

	void createBufferVMA(VmaAllocator& g_vma_Allocator, VkBufferUsageFlags allowedUsage, uint32_t size, VkBuffer& buffer, VmaAllocation& g_vma_BufferAlloc);
	void createVertexandIndexBuffersVMA(VulkanDevice* device, VkCommandPool commandPool, VmaAllocator& g_vma_Allocator,
										const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices,
										VkBuffer& vertexBuffer, VkBuffer& indexBuffer,
										VmaAllocation& g_vma_VertexBufferAlloc, VmaAllocation& g_vma_IndexBufferAlloc);
	void createBufferFromDataVMA(VulkanDevice* device, VkCommandPool commandPool, VmaAllocator& g_vma_Allocator,
								void* bufferData, VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage,
								VkBuffer& vertexBuffer, VmaAllocation& g_vma_IndexBufferAlloc);
}