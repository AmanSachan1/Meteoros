#include "Sky.h"

#define PI_BY_2 1.57f

Sky::Sky(VulkanDevice* device, VkDevice logicalDevice) : device(device), logicalDevice(logicalDevice)
{
	BufferUtils::CreateBuffer(device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(SunAndSky), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sunAndSkyBuffer, sunAndSkyBufferMemory);
	vkMapMemory(device->GetVkDevice(), sunAndSkyBufferMemory, 0, sizeof(SunAndSky), 0, &sunAndSky_mappedData);
	memcpy(sunAndSky_mappedData, &sunAndSky, sizeof(SunAndSky));
}

Sky::~Sky()
{
	delete cloudBaseShapeTexture;
	delete cloudDetailsTexture;
	delete cloudMotionTexture;
	delete weatherMapTexture;

	vkUnmapMemory(device->GetVkDevice(), sunAndSkyBufferMemory);
	vkDestroyBuffer(device->GetVkDevice(), sunAndSkyBuffer, nullptr);
	vkFreeMemory(device->GetVkDevice(), sunAndSkyBufferMemory, nullptr);
}

void Sky::CreateCloudResources(VkCommandPool computeCommandPool)
{
	// Low Frequency Cloud 3D Texture
	const std::string LowFreq_folder_path = "../../src/CloudScapes/textures/CloudTextures/LowFrequency/";
	const std::string LowFreq_textureBaseName = "LowFrequency";
	const std::string LowFreq_fileExtension = ".tga";
	cloudBaseShapeTexture = new Texture3D(device, 128, 128, 128, VK_FORMAT_R8G8B8A8_UNORM);
	cloudBaseShapeTexture->create3DTextureFromMany2DTextures(logicalDevice, computeCommandPool,
		LowFreq_folder_path, LowFreq_textureBaseName, LowFreq_fileExtension,
		128, 4);

	// High Frequency Cloud 3D Texture //TODO Get actual High Frequncy Textures
	const std::string HighFreq_folder_path = "../../src/CloudScapes/textures/CloudTextures/HighFrequency/";
	const std::string HighFreq_textureBaseName = "HighFrequency";
	const std::string HighFreq_fileExtension = ".tga";
	cloudDetailsTexture = new Texture3D(device, 32, 32, 32, VK_FORMAT_R8G8B8A8_UNORM);
	cloudDetailsTexture->create3DTextureFromMany2DTextures(logicalDevice, computeCommandPool,
		HighFreq_folder_path, HighFreq_textureBaseName, HighFreq_fileExtension,
		32, 4);

	// Curl Noise 2D Texture
	const std::string curlNoiseTexture_path = "../../src/CloudScapes/textures/CloudTextures/curlNoise.png";
	cloudMotionTexture = new Texture2D(device, 128, 128, VK_FORMAT_R8G8B8A8_UNORM); //Need to pad an extra channel because R8G8B8 is not supported
	cloudMotionTexture->createTextureFromFile(logicalDevice, computeCommandPool, curlNoiseTexture_path, 4,
		VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SAMPLER_ADDRESS_MODE_REPEAT, 16.0f);

	// Weather Map 2D Texture
	const std::string weatherMapTexture_path = "../../src/CloudScapes/textures/CloudTextures/weatherMap.png";
	weatherMapTexture = new Texture2D(device, 512, 512, VK_FORMAT_R8G8B8A8_UNORM); //Need to pad an extra channel because R8G8B8 is not supported
	weatherMapTexture->createTextureFromFile(logicalDevice, computeCommandPool, weatherMapTexture_path, 4,
		VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SAMPLER_ADDRESS_MODE_REPEAT, 16.0f);
}

VkBuffer Sky::GetSunAndSkyBuffer() const
{
	return sunAndSkyBuffer;
}
void Sky::UpdateSunAndSky()
{
	//float angle = time.frameCount*0.000001f;
	//rotMat = glm::rotate(rotMat, angle, rotationAxis);

	sunAndSky.sunLocation = rotMat * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
	sunAndSky.sunDirection = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
	sunAndSky.lightColor = glm::vec4(1.0f, 1.0f, 0.57f, 1.0f);
	sunAndSky.sunIntensity = 5.0;
	memcpy(sunAndSky_mappedData, &sunAndSky, sizeof(SunAndSky));
}