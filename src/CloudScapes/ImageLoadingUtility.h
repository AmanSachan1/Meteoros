#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "VulkanDevice.h"
#include "Image.h"
#include "Texture3D.h"
#include <cstring>
#include "../../external/stb_image.h"

namespace ImageLoadingUtility
{
	//load an image and upload it into a Vulkan image object
	void loadImageFromFile(VulkanDevice* device, VkCommandPool& commandPool, const char* imagePath,
						VkImage& textureImage, VkDeviceMemory& textureImageMemory, VkFormat format,
						VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
	
	// load multiple 2D Textures From a folder and create a 3D image from them
	void create3DTextureFromMany2DTextures(VulkanDevice* device, VkDevice logicalDevice, VkCommandPool commandPool,
		const std::string folder_path, const std::string textureBaseName, const std::string fileExtension,
		VkImage& texture3DImage, VkDeviceMemory& texture3DMemory, VkFormat textureFormat,
		int width, int height, int depth, int num2DImages, int numChannels);

	void create3DTextureImage(VulkanDevice* device, VkDevice logicalDevice, VkImage& image, VkDeviceMemory& imageMemory,
							VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
							int width, int height, int depth, VkFormat format);
}