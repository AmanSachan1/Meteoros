#include "Texture2D.h"

Texture2D::Texture2D(VulkanDevice* device, uint32_t width, uint32_t height, VkFormat format)
	: device(device), width(width), height(height), textureFormat(format)
{}

Texture2D::~Texture2D()
{
	vkDestroySampler(device->GetVkDevice(), textureSampler, nullptr);
	vkDestroyImageView(device->GetVkDevice(), textureImageView, nullptr);
	vkDestroyImage(device->GetVkDevice(), textureImage, nullptr);
	vkFreeMemory(device->GetVkDevice(), textureImageMemory, nullptr);
}

//This function creates a texture that can be written to
//And thus can be used to prepare a texture target that is used to store compute shader calculations
void Texture2D::createTextureAsBackGround(VkDevice logicalDevice, VkPhysicalDevice physicalDevice, VkCommandPool commandPool)
{
	// Get device properties for the requested texture format
	VkFormatProperties formatProperties;

	vkGetPhysicalDeviceFormatProperties(physicalDevice, textureFormat, &formatProperties);
	// Check if requested image format supports image storage operations
	assert(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);

	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.pNext = NULL;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = textureFormat;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	// Image will be sampled in the fragment shader and used as storage target in the compute shader
	imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

	//The samples flag is related to multisampling. This is only relevant for images that will be used as attachments, so stick to one sample
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.flags = 0; // There are some optional flags for images that are related to sparse images.

	if (vkCreateImage(logicalDevice, &imageInfo, nullptr, &textureImage) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create image!");
	}

	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(logicalDevice, textureImage, &memReqs);

	VkMemoryAllocateInfo memAllocInfo = {};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.pNext = NULL;
	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = device->GetInstance()->GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vkAllocateMemory(logicalDevice, &memAllocInfo, nullptr, &textureImageMemory);
	vkBindImageMemory(logicalDevice, textureImage, textureImageMemory, 0);

	VkCommandBuffer layoutCmd = beginSingleTimeCommands(device, commandPool);
	textureLayout = VK_IMAGE_LAYOUT_GENERAL;
	Image::setImageLayout(layoutCmd, textureImage, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, textureLayout);
	endSingleTimeCommands(device, commandPool, device->GetQueue(QueueFlags::Compute), layoutCmd);

	Image::createSampler(device, textureSampler, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 1.0f);
	Image::createImageView(device, textureImageView, textureImage, textureFormat, VK_IMAGE_ASPECT_COLOR_BIT);
}

void Texture2D::createTextureImage(VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties)
{
	Image::createImage(device, width,  height, textureFormat, tiling, usage, properties, textureImage, textureImageMemory);
}
void Texture2D::createTextureImageView()
{
	Image::createImageView(device, textureImageView, textureImage, textureFormat, VK_IMAGE_ASPECT_COLOR_BIT);
}
void Texture2D::createTextureSampler(VkSamplerAddressMode addressMode, float maxAnisotropy)
{
	Image::createSampler(device, textureSampler, addressMode, maxAnisotropy);
}

uint32_t Texture2D::GetWidth() const
{
	return width;
}
uint32_t Texture2D::GetHeight() const
{
	return height;
}
VkFormat Texture2D::GetTextureFormat() const
{
	return textureFormat;
}
VkImageLayout Texture2D::GetTextureLayout() const
{
	return textureLayout;
}
VkImage Texture2D::GetTextureImage() const
{
	return textureImage;
}
VkDeviceMemory Texture2D::GetTextureImageMemory() const
{
	return textureImageMemory;
}
VkImageView Texture2D::GetTextureImageView() const
{
	return textureImageView;
}
VkSampler Texture2D::GetTextureSampler() const
{
	return textureSampler;
}