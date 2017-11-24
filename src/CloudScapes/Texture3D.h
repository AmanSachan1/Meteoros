#pragma once

#include "VulkanDevice.h"
#include "BufferUtils.h"
#include "Image.h"

class Texture3D
{
public:
	Texture3D() = delete;	// https://stackoverflow.com/questions/5513881/meaning-of-delete-after-function-declaration
	Texture3D(VulkanDevice* device, VkCommandPool commandPool );
	~Texture3D();

private:

};