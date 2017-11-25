#pragma once

#include "VulkanDevice.h"
#include "BufferUtils.h"
#include "Image.h"

class Texture2D
{
public:
	Texture2D() = delete;	// https://stackoverflow.com/questions/5513881/meaning-of-delete-after-function-declaration
	Texture2D(VulkanDevice* device, VkCommandPool commandPool );
	~Texture2D();

	void createTextureImage();
	void createTextureSampler();
	void createTextureImageView();

//private:
	VulkanDevice* device; //member variable because it is needed for the destructor

	int width, height, channels;
	VkFormat textureFormat;
	VkImageLayout textureLayout;

	VkImage textureImage = VK_NULL_HANDLE;
	VkDeviceMemory textureImageMemory = VK_NULL_HANDLE;	
	VkImageView textureImageView = VK_NULL_HANDLE;
	VkSampler textureSampler = VK_NULL_HANDLE;
};