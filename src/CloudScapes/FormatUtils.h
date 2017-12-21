#pragma once

#include <vulkan/vulkan.h>

namespace FormatUtils
{
	// Format Helper Functions
	inline VkFormat FindSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
	{
		for (VkFormat format : candidates)
		{
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

			if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
				return format;
			}
			else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
				return format;
			}
		}

		throw std::runtime_error("failed to find supported format!");
	}

	inline VkFormat FindDepthFormat(VkPhysicalDevice physicalDevice)
	{
		return FindSupportedFormat( physicalDevice, 
									{ VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT }, 
									VK_IMAGE_TILING_OPTIMAL, 
									VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT );
	}
}