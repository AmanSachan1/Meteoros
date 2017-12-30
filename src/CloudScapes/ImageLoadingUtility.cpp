#pragma once
#include "imageLoadingUtility.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../../external/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../external/stb_image_write.h"

void ImageLoadingUtility::loadImageFromFile(VulkanDevice* device, VkCommandPool& commandPool, const char* imagePath,
											VkImage& textureImage, VkDeviceMemory& textureImageMemory, VkFormat format,
											VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties)
{
	//---------------------
	//--- Load an Image ---
	//---------------------
	int texWidth, texHeight, texChannels;
	// The STBI_rgb_alpha value forces the image to be loaded with an alpha channel, even if it doesn't have one, which 
	// is nice for consistency with other textures in the future.
	// The pointer that is returned is the first element in an array of pixel values.
	stbi_uc* pixels = stbi_load(imagePath, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	VkDeviceSize imageSize = texWidth * texHeight * 4; // 4 channel image

	if (!pixels)
	{
		throw std::runtime_error("failed to load texture image!");
	}

	//----------------------
	//--- Staging Buffer ---
	//----------------------		
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	BufferUtils::CreateBuffer(device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, imageSize,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer, stagingBufferMemory);

	//copy the pixel values that we got from the image loading library to the buffer
	{
		void* data;
		vkMapMemory(device->GetVkDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
		memcpy(data, pixels, static_cast<size_t>(imageSize));
		vkUnmapMemory(device->GetVkDevice(), stagingBufferMemory);
	}

	stbi_image_free(pixels);

	Image::createImage(device, texWidth, texHeight, format, tiling, usage, properties, textureImage, textureImageMemory);

	//----------------------------------------------------
	//--- Copy the Staging Buffer to the Texture Image ---
	//----------------------------------------------------
	//The image was created with the VK_IMAGE_LAYOUT_UNDEFINED layout, so that one should be specified as old layout when transitioning textureImage. 
	//Remember that we can do this because we don't care about its contents before performing the copy operation.

	//Transition: Undefined → transfer destination: transfer writes that don't need to wait on anything
	//This transition is to make image optimal as a destination
	Image::transitionImageLayout(device, commandPool, textureImage, format,
								VK_IMAGE_LAYOUT_UNDEFINED,
								VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	Image::copyBufferToImage(device, commandPool, stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

	//Transfer destination → shader reading: shader reads should wait on transfer writes, 
	//specifically the shader reads in the fragment shader, because that's where we're going to use the texture
	//This transition is to make the image an optimal source for the sampler in a shader
	Image::transitionImageLayout(device, commandPool, textureImage, format,
								VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkDestroyBuffer(device->GetVkDevice(), stagingBuffer, nullptr);
	vkFreeMemory(device->GetVkDevice(), stagingBufferMemory, nullptr);
}

// load multiple 2D Textures From a folder and create a 3D image from them
void ImageLoadingUtility::create3DTextureFromMany2DTextures(VulkanDevice* device,VkDevice logicalDevice, VkCommandPool commandPool,
								const std::string folder_path, const std::string textureBaseName, const std::string fileExtension,
								VkImage& texture3DImage, VkDeviceMemory& texture3DMemory, VkFormat textureFormat,
								int width, int height, int depth, int num2DImages, int numChannels)
{
	int texWidth, texHeight, texChannels;
	VkDeviceSize Image3DSize = width * height * depth * numChannels;

	uint8_t* texture3DPixels = new uint8_t[Image3DSize];
	memset(texture3DPixels, 0, Image3DSize);

#pragma omp parallel for
	for (int z = 0; z<num2DImages; z++)
	{
		std::string imageIdentifier = folder_path + textureBaseName + "(" + std::to_string(z + 1) + ")" + fileExtension;
		const char* imagePath = imageIdentifier.c_str();
		stbi_uc* pixels = stbi_load(imagePath, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		if (!pixels) {
			throw std::runtime_error("failed to load texture image!");
		}

		memcpy(&texture3DPixels[z * texWidth * texHeight * 4], pixels, static_cast<size_t>(texWidth * texHeight * 4));
		stbi_image_free(pixels);
	}

	//IMPORTANT: Did not free memory stored in the vector because if we want to modify the individual textures we should just change that one texture	
	// Create the staging buffer
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	VkBufferUsageFlags stagingUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	VkMemoryPropertyFlags stagingProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	BufferUtils::CreateBuffer(device, stagingUsage, Image3DSize, stagingProperties, stagingBuffer, stagingBufferMemory);
	
	// Copy data over to the staging buffer
	void *data;
	vkMapMemory(device->GetVkDevice(), stagingBufferMemory, 0, Image3DSize, 0, &data);
	memcpy(data, texture3DPixels, static_cast<size_t>(Image3DSize));
	vkUnmapMemory(device->GetVkDevice(), stagingBufferMemory);
	
	delete texture3DPixels;

	create3DTextureImage(device, logicalDevice, texture3DImage, texture3DMemory, VK_IMAGE_TILING_OPTIMAL, 
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		width, height, depth, textureFormat);

	//----------------------------------------------------
	//--- Copy the Staging Buffer to the Texture Image ---
	//----------------------------------------------------
	//The image was created with the VK_IMAGE_LAYOUT_UNDEFINED layout, so that one should be specified as old layout when transitioning textureImage. 
	//Remember that we can do this because we don't care about its contents before performing the copy operation.

	//Transition: Undefined → transfer destination: transfer writes that don't need to wait on anything
	//This transition is to make image optimal as a destination
	Image::transitionImageLayout(device, commandPool, texture3DImage, textureFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	Image::copyBufferToImage3D(device, commandPool, stagingBuffer, texture3DImage,
		static_cast<uint32_t>(width), static_cast<uint32_t>(height), static_cast<uint32_t>(depth));

	//Transfer destination → shader reading
	Image::transitionImageLayout(device, commandPool, texture3DImage, textureFormat,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkDestroyBuffer(device->GetVkDevice(), stagingBuffer, nullptr);
	vkFreeMemory(device->GetVkDevice(), stagingBufferMemory, nullptr);
}

void ImageLoadingUtility::create3DTextureImage(VulkanDevice* device, VkDevice logicalDevice, VkImage& image, VkDeviceMemory& imageMemory,
												VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
												int width, int height, int depth, VkFormat format)
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
	imageInfo.format = format; //It is possible that the VK_FORMAT_R8G8B8A8_UNORM format is not supported by the graphics hardware.

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

	if (vkCreateImage(logicalDevice, &imageInfo, nullptr, &image) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create 3D texture!");
	}

	VkMemoryRequirements memReqs = {};
	vkGetImageMemoryRequirements(logicalDevice, image, &memReqs);

	// Device local memory to back up image
	VkMemoryAllocateInfo memAllocInfo{};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = device->GetInstance()->GetMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (vkAllocateMemory(device->GetVkDevice(), &memAllocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate device memory for the buffer");
	}
	vkBindImageMemory(device->GetVkDevice(), image, imageMemory, 0);
}


//----------------------------------------------------------------------------------------------------------------------------------------
//Use this to save raw data out as a ppm file which is a very simple file format and there a re plenty of converters online that can convert ppm to other
//file formats
//Reference: https://stackoverflow.com/questions/13745093/a-portable-function-to-create-a-bmp-file-from-raw-bytes
//http://www.cplusplus.com/reference/cstdio/FILE/

void ImageLoadingUtility::save3DTextureAsImage( const char* output_file_path,
												const std::string input_folder_path, const std::string input_textureBaseName, const std::string input_fileExtension,
												int w, int h, int num2DImages, int numChannels )
{
	int texWidth, texHeight, texChannels;
	size_t Image3DSize = w * h * num2DImages * numChannels;
	uint8_t* texture3DPixels = new uint8_t[Image3DSize];
	memset(texture3DPixels, 0, Image3DSize);
	
#pragma omp parallel for
	for (int z = 0; z<num2DImages; z++)
	{
		std::string imageIdentifier = input_folder_path + input_textureBaseName + "(" + std::to_string(z + 1) + ")" + input_fileExtension;
		const char* imagePath = imageIdentifier.c_str();
		stbi_uc* pixels = stbi_load(imagePath, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		if (!pixels) {
			throw std::runtime_error("failed to load texture image!");
		}

		memcpy(&texture3DPixels[z * texWidth * texHeight * 4], pixels, static_cast<size_t>(texWidth * texHeight * 4));
		stbi_image_free(pixels);
	}
	
	FILE* outfile;
	outfile = fopen(output_file_path, "w");
	
	//use stbi image library to write images
	stbi_write_tga(output_file_path, w, h*num2DImages, 4, texture3DPixels);

	fclose(outfile);
}