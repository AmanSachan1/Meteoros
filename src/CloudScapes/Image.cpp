#pragma once

#include "Image.h"

//Reference: https://vulkan-tutorial.com/Texture_mapping/Images
bool Image::hasStencilComponent(VkFormat format)
{
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void Image::transitionImageLayout(VulkanDevice* device, VkCommandPool commandPool, VkImage& image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	/*
	TODO for Performance: All of the helper functions that submit commands so far have been set up to execute synchronously by waiting 
			for the queue to become idle. For practical applications it is recommended to combine these operations in a single command buffer 
			and execute them asynchronously for higher throughput, especially the transitions and copy in the createImage function. Try 
			to experiment with this by creating a setupCommandBuffer that the helper functions record commands into, and add a flushSetupCommands 
			to execute the commands that have been recorded so far. It's best to do this after the texture mapping works to check if the texture 
			resources are still set up correctly.
	*/

	//command buffer submission results in implicit VK_ACCESS_HOST_WRITE_BIT synchronization at the beginning.
	VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

	/*
	Images can have different layouts that affect how the pixels are organized in memory. Due to the way graphics hardware works,
	simply storing the pixels row by row may not lead to the best performance, for example. When performing any operation on images,
	you must make sure that they have the layout that is optimal for use in that operation. We've actually already seen some of these
	layouts when we specified the render pass:

	VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: Optimal for presentation
	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: Optimal as attachment for writing colors from the fragment shader
	VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: Optimal as source in a transfer operation, like vkCmdCopyImageToBuffer
	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: Optimal as destination in a transfer operation, like vkCmdCopyBufferToImage
	VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: Optimal for sampling from a shader

	One of the most common ways to transition the layout of an image is a pipeline barrier
	*/

	//Pipeline barriers are primarily used for synchronizing access to resources, like making sure that an image was
	//written to before it is read, but they can also be used to transition layouts.

	/*
	One of the most common ways to perform layout transitions is using an image memory barrier.
	A pipeline barrier like that is generally used to synchronize access to resources, like ensuring that a write
	to a buffer completes before reading from it, but it can also be used to transition image layouts and transfer
	queue family ownership when VK_SHARING_MODE_EXCLUSIVE is used.
	*/
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	//specify layout transition
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	//If you are using the barrier to transfer queue family ownership, then these two fields should be the indices
	//of the queue families. They must be set to VK_QUEUE_FAMILY_IGNORED if you don't want to do this (not the default value!).
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	// The image and subresourceRange specify the image that is affected and the specific part of the image.
	// Our image is not an array and does not use mipmapping levels, so only one level and layer are specified.
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	//Barriers are primarily used for synchronization purposes, so you must specify which types of operations 
	//that involve the resource must happen before the barrier, and which operations that involve the resource
	//must wait on the barrier. We need to do that despite already using vkQueueWaitIdle to manually synchronize.
	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	// TODO: If adding more transitions add the specific cases to the if else ladder
	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && 
		newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		//Since the writes don't have to wait on anything, you may specify an empty access mask and the
		//earliest possible pipeline stage VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT for the pre-barrier operations.
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && 
			 newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && 
			 newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) 
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | 
								VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		/*
		The depth buffer will be read from to perform depth tests to see if a fragment is visible, 
		and will be written to when a new fragment is drawn. The reading happens in the 
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT stage and the writing in the 
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT. You should pick the earliest pipeline stage that 
		matches the specified operations, so that it is ready for usage as depth attachment when it needs to be.
		*/

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	else
	{
		throw std::invalid_argument("unsupported layout transition!");
	}

	if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) 
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

		if (hasStencilComponent(format)) 
		{
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
	}
	else 
	{
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	//Resource https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#synchronization-access-types-supported
	// All types of pipeline barriers are submitted using the same function.
	vkCmdPipelineBarrier(commandBuffer,
						sourceStage, //operationsbefore the barrier occur in which pipeline stage
						destinationStage, //which pipeline stage waits for the barrier
						0, //The third parameter is either 0 or VK_DEPENDENCY_BY_REGION_BIT. The latter turns the barrier into a per-region condition.
							//That means that the implementation is allowed to already begin reading from the parts of a resource that were written so far, for example.
							//The last three pairs of parameters reference arrays of pipeline barriers of the three available types : memory barriers, 
							//buffer memory barriers, and image memory barriers like the one we're using here.
						0, nullptr,
						0, nullptr,
						1, &barrier);

	endSingleTimeCommands(device, commandPool, commandBuffer);
}

void Image::copyBufferToImage(VulkanDevice* device, VkCommandPool commandPool, VkBuffer buffer, VkImage& image, uint32_t width, uint32_t height )
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

	//VkBufferImageCopy struct specifies which part of the buffer is going to be copied to which part of the image
	VkBufferImageCopy region = {};
	region.bufferOffset = 0; //byte offset in the buffer at which the pixel values start
							 //The bufferRowLength and bufferImageHeight fields specify how the pixels are laid out in memory. 
							 //For example, you could have some padding bytes between rows of the image. Specifying 0 for both indicates 
							 //that the pixels are simply tightly packed like they are in our case.
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	//imageSubresource is a VkImageSubresourceLayers used to specify the specific image subresources of the image used
	//for the source or destination image data.
	//Subresource --> sub-section of the image that is used 
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

	//The imageSubresource, imageOffset and imageExtent fields indicate to which part of the image we want to copy the pixels
	region.imageOffset = { 0,0,0 };
	region.imageExtent = { width, height, 1 };

	//layout: assuming that the image has already been transitioned to the layout that is optimal for copying pixels to
	//RegionCount: Right now we're only copying one chunk of pixels to the whole image, but it's possible to specify an 
	//			   array of VkBufferImageCopy to perform many different copies from this buffer to the image in one operation.
	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	endSingleTimeCommands(device, commandPool, commandBuffer);
}

void Image::copyBufferToImage3D(VulkanDevice* device, VkCommandPool commandPool, VkBuffer buffer, VkImage& image, uint32_t width, uint32_t height, uint32_t depth)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

	//VkBufferImageCopy struct specifies which part of the buffer is going to be copied to which part of the image
	VkBufferImageCopy region = {};
	region.bufferOffset = 0; //byte offset in the buffer at which the pixel values start
							 //The bufferRowLength and bufferImageHeight fields specify how the pixels are laid out in memory. 
							 //For example, you could have some padding bytes between rows of the image. Specifying 0 for both indicates 
							 //that the pixels are simply tightly packed like they are in our case.
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	//imageSubresource is a VkImageSubresourceLayers used to specify the specific image subresources of the image used
	//for the source or destination image data.
	//Subresource --> sub-section of the image that is used 
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

	//The imageSubresource, imageOffset and imageExtent fields indicate to which part of the image we want to copy the pixels
	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = { width, height, depth };

	//layout: assuming that the image has already been transitioned to the layout that is optimal for copying pixels to
	//RegionCount: Right now we're only copying one chunk of pixels to the whole image, but it's possible to specify an 
	//			   array of VkBufferImageCopy to perform many different copies from this buffer to the image in one operation.
	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	endSingleTimeCommands(device, commandPool, commandBuffer);
}

void Image::createImage(VulkanDevice* device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
						VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
{
	//-------------
	//--- Image ---
	//-------------

	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D; //tells Vulkan with what kind of coordinate system the texels in the image are going to be addressed.
											//It is possible to create 1D, 2D and 3D images
											// extent defines image dimensions
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
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

	if (vkCreateImage(device->GetVkDevice(), &imageInfo, nullptr, &image) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create image!");
	}

	//-----------------------------------
	//--- Allocating Memory for image ---
	//-----------------------------------
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device->GetVkDevice(), image, &memRequirements);

	imageMemory = BufferUtils::CreateDeviceMemory(device, memRequirements.size,
												memRequirements.memoryTypeBits,
												properties);

	vkBindImageMemory(device->GetVkDevice(), image, imageMemory, 0);
}

/*
	Resource: https://vulkan-tutorial.com/Texture_mapping/Image_view_and_sampler
*/
void Image::createImageView(VulkanDevice* device, VkImageView& imageView, VkImage& textureImage,
							VkFormat format, VkImageAspectFlags aspectFlags)
{
	// Create the image view 
	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = textureImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;

	// Describe the image's purpose and which part of the image should be accessed
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(device->GetVkDevice(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) 
	{
		throw std::runtime_error("failed to create texture image view!");
	}
}

/*
	Resource: https://vulkan-tutorial.com/Texture_mapping/Image_view_and_sampler
*/
void Image::createSampler(VulkanDevice* device, VkSampler& sampler, VkSamplerAddressMode addressMode, float maxAnisotropy)
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

	if (vkCreateSampler(device->GetVkDevice(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture sampler!");
	}
}

void Image::setImageLayout( VkCommandBuffer cmdbuffer,
							VkImage image,
							VkImageAspectFlags aspectMask,
							VkImageLayout oldImageLayout,
							VkImageLayout newImageLayout)
{
	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = aspectMask;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.layerCount = 1;

	// Create an image barrier object
	VkImageMemoryBarrier imageMemoryBarrier = {};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.pNext = NULL;
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.oldLayout = oldImageLayout;
	imageMemoryBarrier.newLayout = newImageLayout;
	imageMemoryBarrier.image = image;
	imageMemoryBarrier.subresourceRange = subresourceRange;

	// Source layouts (old)
	// Source access mask controls actions that have to be finished on the old layout
	// before it will be transitioned to the new layout
	switch (oldImageLayout)
	{
	case VK_IMAGE_LAYOUT_UNDEFINED:
		// Image layout is undefined (or does not matter)
		// Only valid as initial layout
		// No flags required, listed only for completeness
		imageMemoryBarrier.srcAccessMask = 0;
		break;

	case VK_IMAGE_LAYOUT_PREINITIALIZED:
		// Image is preinitialized
		// Only valid as initial layout for linear images, preserves memory contents
		// Make sure host writes have been finished
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		// Image is a color attachment
		// Make sure any writes to the color buffer have been finished
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		// Image is a depth/stencil attachment
		// Make sure any writes to the depth/stencil buffer have been finished
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		// Image is a transfer source 
		// Make sure any reads from the image have been finished
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		// Image is a transfer destination
		// Make sure any writes to the image have been finished
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		// Image is read by a shader
		// Make sure any shader reads from the image have been finished
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		break;
	}

	// Target layouts (new)
	// Destination access mask controls the dependency for the new image layout
	switch (newImageLayout)
	{
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		// Image will be used as a transfer destination
		// Make sure any writes to the image have been finished
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		// Image will be used as a transfer source
		// Make sure any reads from and writes to the image have been finished
		imageMemoryBarrier.srcAccessMask = imageMemoryBarrier.srcAccessMask | VK_ACCESS_TRANSFER_READ_BIT;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		break;

	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		// Image will be used as a color attachment
		// Make sure any writes to the color buffer have been finished
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		// Image layout will be used as a depth/stencil attachment
		// Make sure any writes to depth/stencil buffer have been finished
		imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		// Image will be read in a shader (sampler, input attachment)
		// Make sure any writes to the image have been finished
		if (imageMemoryBarrier.srcAccessMask == 0)
		{
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
		}
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		break;
	}

	// Put barrier on top
	VkPipelineStageFlags srcStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	VkPipelineStageFlags destStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

	// Put barrier inside setup command buffer
	vkCmdPipelineBarrier(
		cmdbuffer,
		srcStageFlags,
		destStageFlags,
		0,
		0, nullptr,
		0, nullptr,
		1, &imageMemoryBarrier);
}