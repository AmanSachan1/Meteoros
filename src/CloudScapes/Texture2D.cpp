#include "Texture2D.h"

Texture2D::Texture2D(VulkanDevice* device, VkCommandPool commandPool)
{

}

Texture2D::~Texture2D()
{

}

void Texture2D::createTextureImage()
{

}
void Texture2D::createTextureImageView()
{
	Image::createImageView(device, textureImageView, textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
}
void Texture2D::createTextureSampler()
{
	Image::createSampler(device, textureSampler, VK_SAMPLER_ADDRESS_MODE_REPEAT, 16.0f);
}