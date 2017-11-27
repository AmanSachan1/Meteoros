#pragma once
#include "imageLoadingUtility.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../../external/stb_image.h"

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

void ImageLoadingUtility::loadmultiple2DTextures(std::vector <unsigned char*>& texture2DPixels,
												const std::string folder_path, const std::string textureBaseName,
												const std::string fileExtension, int num2DImages,
												int& texWidth, int& texHeight, int& texChannels)
{
	//---------------------
	//--- Load Images -----
	//---------------------
	// The STBI_rgb_alpha value forces the image to be loaded with an alpha channel, even if it doesn't have one, which 
	// is nice for consistency with other textures in the future.
	// The pointer that is returned is the first element in an array of pixel values.
	for (int i = 0; i<num2DImages; i++)
	{
		std::string imageIdentifier = folder_path + textureBaseName + " (" + std::to_string(i + 1) + ")" + fileExtension;
		const char* imagePath = imageIdentifier.c_str();
		stbi_uc* pixels = stbi_load(imagePath, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		texture2DPixels.push_back(static_cast<unsigned char*>(pixels));
		if (!texture2DPixels[i])
		{
			throw std::runtime_error("failed to load texture image!");
		}

		stbi_image_free(pixels);
	}
}