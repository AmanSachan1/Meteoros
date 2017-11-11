#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

VkShaderModule createShaderModule(const std::vector<char>& code, VkDevice logicalDevice);
VkShaderModule createShaderModule(const std::string& filename, VkDevice logicalDevice);