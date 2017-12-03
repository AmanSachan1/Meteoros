#include "Texture3D.h"

Texture3D::Texture3D(VulkanDevice* device, uint32_t width, uint32_t height, uint32_t depth, VkFormat format)
	: device(device), width(width), height(height), depth(depth), textureFormat(format)
{}

Texture3D::~Texture3D()
{
	if (textureSampler3D != VK_NULL_HANDLE) {
		vkDestroySampler(device->GetVkDevice(), textureSampler3D, nullptr);
	}
	if (textureImageView3D != VK_NULL_HANDLE) {
		vkDestroyImageView(device->GetVkDevice(), textureImageView3D, nullptr);
	}
	if (textureImage3D != VK_NULL_HANDLE) {
		vkDestroyImage(device->GetVkDevice(), textureImage3D, nullptr);
	}
	if (textureImageMemory3D != VK_NULL_HANDLE) {
		vkFreeMemory(device->GetVkDevice(), textureImageMemory3D, nullptr);
	}
}

void Texture3D::create3DTexture(VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
	VkSamplerAddressMode addressMode, float maxAnisotropy)
{
	create3DTextureImage(tiling, usage, properties);
	create3DTextureImageView();
	create3DTextureSampler(addressMode, maxAnisotropy);
}

void Texture3D::create3DTextureImage(VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties)
{
	//-------------
	//--- Image ---
	//-------------

	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_3D; //tells Vulkan with what kind of coordinate system the texels in the image are going to be addressed.
											// extent defines image dimensions
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = depth;
	//Our texture will not be an array and we won't be using mipmapping for now.
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;

	//use the same format for the texels as the pixels in the buffer, otherwise the copy operation will fail.
	imageInfo.format = textureFormat; //It is possible that the VK_FORMAT_R8G8B8A8_UNORM format is not supported by the graphics hardware.

									  //2 types of memory arrangemnets for tiling: linear (row-major) and optimal (implementation defined order for optimal access)
									  //the tiling mode cannot be changed at a later time
									  //If you want to be able to directly access texels in the memory of the image, then you must use VK_IMAGE_TILING_LINEAR.
									  //We will be using a staging buffer instead of a staging image, so this won't be necessary. 
									  //We will be using VK_IMAGE_TILING_OPTIMAL for efficient access from the shader.
	imageInfo.tiling = tiling;

	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Not usable by the GPU and the very first transition will discard the texels.
														 // very few situations where it is necessary for the texels to be preserved 
														 // during the first transition

														 //The image is going to be used as destination for the buffer copy, so it should be set up as a transfer destination. 
														 // We also want to be able to access the image from the shader to color our mesh, so the usage should include VK_IMAGE_USAGE_SAMPLED_BIT
	imageInfo.usage = usage;

	//The image will only be used by one queue family: the one that supports graphics (and therefore also) transfer operations.
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	//The samples flag is related to multisampling. This is only relevant for images that will be used as attachments, so stick to one sample
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.flags = 0; // There are some optional flags for images that are related to sparse images.

	if (vkCreateImage(device->GetVkDevice(), &imageInfo, nullptr, &textureImage3D) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create 3D texture!");
	}

	VkMemoryRequirements memReqs = {};
	vkGetImageMemoryRequirements(device->GetVkDevice(), textureImage3D, &memReqs);

	// Device local memory to back up image
	VkMemoryAllocateInfo memAllocInfo{};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = device->GetInstance()->GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	
	if (vkAllocateMemory(device->GetVkDevice(), &memAllocInfo, nullptr, &textureImageMemory3D) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate device memory for the buffer");
	}
	vkBindImageMemory(device->GetVkDevice(), textureImage3D, textureImageMemory3D, 0);
}
void Texture3D::create3DTextureSampler(VkSamplerAddressMode addressMode, float maxAnisotropy)
{
	// Create Texture Sampler 
	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

	samplerInfo.magFilter = VK_FILTER_LINEAR;    // Specify how to interpolate texels that are magnified (concerns oversampling problem, aka more fragments than texels)
	samplerInfo.minFilter = VK_FILTER_LINEAR;    // Specify how to interpolate texels that are minified (concerns undersampling problem, aka more texels than fragments)

												 // Repeat the texture when going beyond the image dimensions
	samplerInfo.addressModeU = addressMode;// VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = addressMode;// VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = addressMode;// VK_SAMPLER_ADDRESS_MODE_REPEAT;

										   // Specifies to use anisotropic filtering
										   // Limits amount of texel samples that can be used to calculate final color (lower value, better performance, but lower quality)
										   // No graphics hardware that will use more than 16 samples
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = maxAnisotropy;

	// Specifies which color is returned when sampling beyond the image with clamp to border addressing mode
	// Only black, white, or transparent available
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

	// Specifies which coordinate system to use to address texels in image
	// True --> use coords within [0, texWidth) and [0, texHeight)
	// False --> [0, 1) on all axes
	samplerInfo.unnormalizedCoordinates = VK_FALSE;

	// If true, texels will be first compared to a value, and result of comparison is used in filtering operations (mainly for percentage-closer filtering on shadow maps)
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

	// Mipmapping 
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;

	if (vkCreateSampler(device->GetVkDevice(), &samplerInfo, nullptr, &textureSampler3D) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture sampler!");
	}
}
void Texture3D::create3DTextureImageView()
{
	// Create the image view 
	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = textureImage3D;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
	viewInfo.format = textureFormat;

	// Describe the image's purpose and which part of the image should be accessed
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(device->GetVkDevice(), &viewInfo, nullptr, &textureImageView3D) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create 3D texture image view!");
	}
}

void Texture3D::create3DTextureFromMany2DTextures(VkDevice logicalDevice, VkCommandPool commandPool,
	const std::string folder_path, const std::string textureBaseName, const std::string fileExtension,
	int num2DImages, int numChannels)
{
	ImageLoadingUtility::create3DTextureFromMany2DTextures(device, logicalDevice, commandPool, folder_path, textureBaseName, fileExtension,
		textureImage3D, textureImageMemory3D, textureFormat, width, height, depth, num2DImages, numChannels);

	create3DTextureSampler(VK_SAMPLER_ADDRESS_MODE_REPEAT, 16.0f);
	create3DTextureImageView();
}

uint32_t Texture3D::GetWidth() const
{
	return width;
}
uint32_t Texture3D::GetHeight() const
{
	return height;
}
uint32_t Texture3D::GetDepth() const
{
	return depth;
}
VkFormat Texture3D::GetTextureFormat() const
{
	return textureFormat;
}
VkImageLayout Texture3D::GetTextureLayout() const
{
	return textureLayout;
}
VkImage Texture3D::GetTextureImage()
{
	return textureImage3D;
}
VkDeviceMemory Texture3D::GetTextureImageMemory()
{
	return textureImageMemory3D;
}
VkImageView Texture3D::GetTextureImageView()
{
	return textureImageView3D;
}
VkSampler Texture3D::GetTextureSampler()
{
	return textureSampler3D;
}