#pragma once

#include "VulkanDevice.h"
#include "BufferUtils.h"
#include "imageLoadingUtility.h"
#include "Image.h"

class Texture2D
{
public:
	Texture2D() = delete;	// https://stackoverflow.com/questions/5513881/meaning-of-delete-after-function-declaration
	Texture2D(VulkanDevice* device, uint32_t width, uint32_t height, VkFormat format);
	~Texture2D();

	void create2DTexture(VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
						 VkSamplerAddressMode addressMode, float maxAnisotropy);
	void createTextureImage(VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
	void createTextureSampler(VkSamplerAddressMode addressMode, float maxAnisotropy);
	void createTextureImageView();

	void createEmptyTexture(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool commandPool);
	void createTextureFromFile(VkDevice logicalDevice, VkCommandPool commandPool, const std::string texture_path, int numChannels,
								VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, 
								VkSamplerAddressMode addressMode, float maxAnisotropy);

	uint32_t GetWidth() const;
	uint32_t GetHeight() const;
	VkFormat GetTextureFormat() const;
	VkImageLayout GetTextureLayout() const;
	VkImage GetTextureImage() const;
	VkDeviceMemory GetTextureImageMemory() const;
	VkImageView GetTextureImageView() const;
	VkSampler GetTextureSampler() const;
private:
	VulkanDevice* device; //member variable because it is needed for the destructor

	uint32_t width, height;
	VkFormat textureFormat;
	VkImageLayout textureLayout;

	VkImage textureImage = VK_NULL_HANDLE;
	VkDeviceMemory textureImageMemory = VK_NULL_HANDLE;	
	VkImageView textureImageView = VK_NULL_HANDLE;
	VkSampler textureSampler = VK_NULL_HANDLE;
};