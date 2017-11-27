#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "VulkanDevice.h"
#include "Commands.h"
#include "Vertex.h"
#include "../../external/vk_mem_alloc.h"

#define ERR_GUARD_VULKAN(Expr) do { VkResult res__ = (Expr); if (res__ < 0) assert(0); } while(0)

namespace VMA_Utility
{
	void createBufferVMA(VmaAllocator& g_vma_Allocator, VkBufferUsageFlags allowedUsage, uint32_t size, VkBuffer& buffer, VmaAllocation& g_vma_BufferAlloc);
	void createVertexandIndexBuffersVMA(VulkanDevice* device, VkCommandPool commandPool, VmaAllocator& g_vma_Allocator,
										const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices,
										VkBuffer& vertexBuffer, VkBuffer& indexBuffer,
										VmaAllocation& g_vma_VertexBufferAlloc, VmaAllocation& g_vma_IndexBufferAlloc);
	void createBufferFromDataVMA(VulkanDevice* device, VkCommandPool commandPool, VmaAllocator& g_vma_Allocator,
								void* bufferData, VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage,
								VkBuffer& vertexBuffer, VmaAllocation& g_vma_IndexBufferAlloc);

	void createImageVMA(VmaAllocator& g_vma_Allocator, VkImageCreateInfo& imageInfo, VmaAllocationCreateInfo& imageAllocCreateInfo,
						VkImage& image, VmaAllocation& ImageAlloc, VmaAllocationInfo allocInfo);
}