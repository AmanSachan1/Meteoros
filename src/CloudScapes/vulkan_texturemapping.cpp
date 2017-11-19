#include "vulkan_texturemapping.h"
#include "vulkan_buffer.h"

//Reference: https://vulkan-tutorial.com/Texture_mapping/Images

VkCommandBuffer beginSingleTimeCommands(VulkanDevice* device, VkCommandPool commandPool)
{
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(device->GetVulkanDevice(), &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void endSingleTimeCommands(VulkanDevice* device, VkCommandPool commandPool, VkCommandBuffer commandBuffer)
{
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(device->GetQueue(QueueFlags::Graphics), 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(device->GetQueue(QueueFlags::Graphics));

	vkFreeCommandBuffers(device->GetVulkanDevice(), commandPool, 1, &commandBuffer);
}

void copyBuffer(VulkanDevice* device, VkCommandPool commandPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

	VkBufferCopy copyRegion = {};
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	endSingleTimeCommands(device, commandPool, commandBuffer);
}

void transitionImageLayout(VulkanDevice* device, VkCommandPool commandPool, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	/*
	TODO for Performance: All of the helper functions that submit commands so far have been set up to execute synchronously by waiting 
			for the queue to become idle. For practical applications it is recommended to combine these operations in a single command buffer 
			and execute them asynchronously for higher throughput, especially the transitions and copy in the createTextureImage function. Try 
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
	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		//Since the writes don't have to wait on anything, you may specify an empty access mask and the
		//earliest possible pipeline stage VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT for the pre-barrier operations.
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else
	{
		throw std::invalid_argument("unsupported layout transition!");
	}

	//Resource https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#synchronization-access-types-supported
	// All types of pipeline barriers are submitted using the same function.
	vkCmdPipelineBarrier(
		commandBuffer,
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

void copyBufferToImage(VulkanDevice* device, VkCommandPool commandPool, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
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
	region.imageExtent = { width, height, 1 };

	//layout: assuming that the image has already been transitioned to the layout that is optimal for copying pixels to
	//RegionCount: Right now we're only copying one chunk of pixels to the whole image, but it's possible to specify an 
	//			   array of VkBufferImageCopy to perform many different copies from this buffer to the image in one operation.
	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	endSingleTimeCommands(device, commandPool, commandBuffer);
}

void createImage(VulkanDevice* device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
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
	imageInfo.extent.width = static_cast<uint32_t>(width);
	imageInfo.extent.height = static_cast<uint32_t>(height);
	imageInfo.extent.depth = 1;
	//Our texture will not be an array and we won't be using mipmapping for now.
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;

	//use the same format for the texels as the pixels in the buffer, otherwise the copy operation will fail.
	imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM; //It is possible that the VK_FORMAT_R8G8B8A8_UNORM format is not supported by the graphics hardware.

												 //2 types of memory arrangemnets for tiling: linear (row-major) and optimal (implementation defined order for optimal access)
												 //the tiling mode cannot be changed at a later time
												 //If you want to be able to directly access texels in the memory of the image, then you must use VK_IMAGE_TILING_LINEAR.
												 //We will be using a staging buffer instead of a staging image, so this won't be necessary. 
												 //We will be using VK_IMAGE_TILING_OPTIMAL for efficient access from the shader.
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;

	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Not usable by the GPU and the very first transition will discard the texels.
														 // very few situations where it is necessary for the texels to be preserved 
														 // during the first transition

														 //The image is going to be used as destination for the buffer copy, so it should be set up as a transfer destination. 
														 // We also want to be able to access the image from the shader to color our mesh, so the usage should include VK_IMAGE_USAGE_SAMPLED_BIT
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	//The image will only be used by one queue family: the one that supports graphics (and therefore also) transfer operations.
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	//The samples flag is related to multisampling. This is only relevant for images that will be used as attachments, so stick to one sample
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.flags = 0; // There are some optional flags for images that are related to sparse images.

	if (vkCreateImage(device->GetVulkanDevice(), &imageInfo, nullptr, &image) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to create image!");
	}

	//-----------------------------------
	//--- Allocating Memory for image ---
	//-----------------------------------
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device->GetVulkanDevice(), image, &memRequirements);

	imageMemory = CreateDeviceMemory(device, memRequirements.size,
									memRequirements.memoryTypeBits,
									VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vkBindImageMemory(device->GetVulkanDevice(), image, imageMemory, 0);
}

void createTextureImage(VulkanDevice* device, VkCommandPool commandPool)
{
	// TODO: Make this generalized for any image by taking in the image path!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	//---------------------
	//--- Load an Image ---
	//---------------------
	int texWidth, texHeight, texChannels;
	// The STBI_rgb_alpha value forces the image to be loaded with an alpha channel, even if it doesn't have one, which 
	// is nice for consistency with other textures in the future.
	// The pointer that is returned is the first element in an array of pixel values.
	stbi_uc* pixels = stbi_load("textures/meghanatheminion.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	VkDeviceSize imageSize = texWidth * texHeight * 4; // 4 channel image

	if (!pixels)
	{
		throw std::runtime_error("failed to load texture image!");
	}

	//----------------------
	//--- Staging Buffer ---
	//----------------------
	unsigned int textureBufferOffsets[1];
	VkBuffer stagingBuffer = CreateBuffer(device, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, imageSize);
	VkDeviceMemory stagingBufferMemory = AllocateMemoryForBuffers(device, { stagingBuffer },
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		textureBufferOffsets);

	//copy the pixel values that we got from the image loading library to the buffer
	{
		void* data;
		vkMapMemory(device->GetVulkanDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
		memcpy(data, pixels, static_cast<size_t>(imageSize));
		vkUnmapMemory(device->GetVulkanDevice(), stagingBufferMemory);
	}

	stbi_image_free(pixels);

	//to be cleaned up in main
	VkImage textureImage;
	VkDeviceMemory textureImageMemory;

	createImage(device, texWidth, texHeight, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				textureImage, textureImageMemory);

	//----------------------------------------------------
	//--- Copy the Staging Buffer to the Texture Image ---
	//----------------------------------------------------
	//The image was created with the VK_IMAGE_LAYOUT_UNDEFINED layout, so that one should be specified as old layout when transitioning textureImage. 
	//Remember that we can do this because we don't care about its contents before performing the copy operation.
	
	//Transition: Undefined → transfer destination: transfer writes that don't need to wait on anything
	//This transition is to make image optimal as a destination
	transitionImageLayout(device, commandPool, textureImage, VK_FORMAT_R8G8B8A8_UNORM, 
						VK_IMAGE_LAYOUT_UNDEFINED, 
						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	copyBufferToImage( device, commandPool, stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
	
	//Transfer destination → shader reading: shader reads should wait on transfer writes, 
	//specifically the shader reads in the fragment shader, because that's where we're going to use the texture
	//This transition is to make the image an optimal source for the sampler in a shader
	transitionImageLayout(device, commandPool, textureImage, VK_FORMAT_R8G8B8A8_UNORM, 
						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkDestroyBuffer(device->GetVulkanDevice(), stagingBuffer, nullptr);
	vkFreeMemory(device->GetVulkanDevice(), stagingBufferMemory, nullptr);
}