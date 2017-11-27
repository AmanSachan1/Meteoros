#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "VulkanDevice.h"
#include "Image.h"
#include "../../external/stb_image.h"

namespace ImageLoadingUtility
{
	//load an image and upload it into a Vulkan image object
	void loadImageFromFile(VulkanDevice* device, VkCommandPool& commandPool, const char* imagePath,
						VkImage& textureImage, VkDeviceMemory& textureImageMemory, VkFormat format,
						VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);

	void loadmultiple2DTextures(std::vector <unsigned char*>& texture2DPixels,
								const std::string folder_path, const std::string textureBaseName,
								const std::string fileExtension, int num2DImages,
								int& texWidth, int& texHeight, int& texChannels);
}