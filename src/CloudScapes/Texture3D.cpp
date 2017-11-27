#include "Texture3D.h"

Texture3D::Texture3D(VulkanDevice* device, VmaAllocator g_vma_Allocator, uint32_t width, uint32_t height, uint32_t depth, VkFormat format)
	: device(device), g_vma_Allocator(g_vma_Allocator), width(width), height(height), depth(depth), textureFormat(format)
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

	VmaAllocationCreateInfo imageAllocCreateInfo = {};
	imageAllocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	VmaAllocationInfo allocInfo;

	VMA_Utility::createImageVMA(g_vma_Allocator, imageInfo, imageAllocCreateInfo, textureImage3D, vma_TextureImageAlloc, allocInfo);
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

// load multiple 2D Textures From a folder and create a 3D image from them
void Texture3D::create3DTextureFromMany2DTextures(VkCommandPool commandPool, VmaAllocator& g_vma_Allocator,
	const std::string folder_path, const std::string textureBaseName,
	const std::string fileExtension, int num2DImages, int numChannels)
{
	int texWidth, texHeight, texChannels;
	ImageLoadingUtility::loadmultiple2DTextures(texture2DPixels, folder_path, textureBaseName, fileExtension,
												num2DImages, texWidth, texHeight, texChannels);

	//----------------------
	//--- Staging Buffer ---
	//----------------------		
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	//assuming the 2D images have the same width and height
	VkDeviceSize Image3DSize = texWidth * texHeight * numChannels * num2DImages; // 4 channel image

	BufferUtils::CreateBuffer(device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, Image3DSize,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer, stagingBufferMemory);

	//BufferUtils::createBufferFromDataVMA(device, commandPool, g_vma_Allocator,
	//	texture2DPixels.data(), Image3DSize, VK_IMAGE_USAGE_SAMPLED_BIT,
	//	VkBuffer& buffer, VmaAllocation& g_vma_BufferAlloc);

	//copy the pixel values for all the 2D textures that we got from all the images loaded using the library to the buffer
	{
		void* data;
		vkMapMemory(device->GetVkDevice(), stagingBufferMemory, 0, Image3DSize, 0, &data);
		memcpy(data, texture2DPixels.data(), static_cast<size_t>(Image3DSize));
		vkUnmapMemory(device->GetVkDevice(), stagingBufferMemory);
	}

	//IMPORTANT: Did not free memory stored in the vector because if we want to modify the individual textures we should just change that one texture
	create3DTextureImage(VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//----------------------------------------------------
	//--- Copy the Staging Buffer to the Texture Image ---
	//----------------------------------------------------
	//The image was created with the VK_IMAGE_LAYOUT_UNDEFINED layout, so that one should be specified as old layout when transitioning textureImage. 
	//Remember that we can do this because we don't care about its contents before performing the copy operation.

	//Transition: Undefined → transfer destination: transfer writes that don't need to wait on anything
	//This transition is to make image optimal as a destination
	Image::transitionImageLayout(device, commandPool, textureImage3D, textureFormat,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	Image::copyBufferToImage(device, commandPool, stagingBuffer, textureImage3D, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

	//Transfer destination → shader reading: shader reads should wait on transfer writes, 
	//specifically the shader reads in the fragment shader, because that's where we're going to use the texture
	//This transition is to make the image an optimal source for the sampler in a shader
	Image::transitionImageLayout(device, commandPool, textureImage3D, textureFormat,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkDestroyBuffer(device->GetVkDevice(), stagingBuffer, nullptr);
	vkFreeMemory(device->GetVkDevice(), stagingBufferMemory, nullptr);

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
VkImage Texture3D::GetTextureImage() const
{
	return textureImage3D;
}
VkDeviceMemory Texture3D::GetTextureImageMemory() const
{
	return textureImageMemory3D;
}
VkImageView Texture3D::GetTextureImageView() const
{
	return textureImageView3D;
}
VkSampler Texture3D::GetTextureSampler() const
{
	return textureSampler3D;
}