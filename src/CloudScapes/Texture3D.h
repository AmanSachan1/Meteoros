#pragma once

#include "VulkanDevice.h"
#include "BufferUtils.h"
#include "Image.h"

class Texture3D
{
public:
	Texture3D() = delete;	// https://stackoverflow.com/questions/5513881/meaning-of-delete-after-function-declaration
	Texture3D(VulkanDevice* device, uint32_t width, uint32_t height, uint32_t depth, VkFormat format);
	~Texture3D();

	void create3DTexture(VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
						 VkSamplerAddressMode addressMode, float maxAnisotropy);
	void create3DTextureImage(VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
	void create3DTextureSampler(VkSamplerAddressMode addressMode, float maxAnisotropy);
	void create3DTextureImageView();

	void create3DTextureFromMany2DTextures(VkCommandPool commandPool, 
		const std::string folder_path, const std::string textureBaseName, 
		const std::string fileExtension, int num2DImages, int numChannels);

	uint32_t GetWidth() const;
	uint32_t GetHeight() const;
	uint32_t GetDepth() const;
	VkFormat GetTextureFormat() const;
	VkImageLayout GetTextureLayout() const;
	VkImage GetTextureImage() const;
	VkDeviceMemory GetTextureImageMemory() const;
	VkImageView GetTextureImageView() const;
	VkSampler GetTextureSampler() const;
private:
	VulkanDevice* device; //member variable because it is needed for the destructor

	std::vector <unsigned char*> texture2DPixels; // stores the data stored by the image loading library

	uint32_t width, height, depth;
	VkFormat textureFormat;
	VkImageLayout textureLayout;

	VkImage textureImage3D = VK_NULL_HANDLE;
	VkDeviceMemory textureImageMemory3D = VK_NULL_HANDLE;
	VkImageView textureImageView3D = VK_NULL_HANDLE;
	VkSampler textureSampler3D = VK_NULL_HANDLE;
};