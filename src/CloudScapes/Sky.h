#pragma once

#include <glm/glm.hpp>
#include "VulkanDevice.h"
#include "BufferUtils.h"
#include "Texture2D.h"
#include "Texture3D.h"

struct SunAndSky
{
	glm::vec4 sunLocation = glm::vec4(0.0, 1.0, -10.0, 0.0f);
	glm::vec4 sunDirection = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
	glm::vec4 lightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	float sunIntensity = 1.0;
};

class Sky
{
private:
	VulkanDevice* device;
	VkDevice logicalDevice;

	SunAndSky sunAndSky;
	VkBuffer sunAndSkyBuffer;
	VkDeviceMemory sunAndSkyBufferMemory;
	void* sunAndSky_mappedData;

	glm::vec3 rotationAxis = glm::vec3(1, 0, 0);
	glm::mat4 rotMat = glm::mat4(1.0f);

public:
	Sky() = delete;
	Sky(VulkanDevice* device, VkDevice logicalDevice);
	~Sky();

	Texture2D* weatherMapTexture;
	Texture3D* cloudBaseShapeTexture;
	Texture3D* cloudDetailsTexture;
	Texture2D* cloudMotionTexture;
	/*
	3D cloudBaseShapeTexture
	4 channels…
	128^3 resolution…
	The first channel is the Perlin-Worley noise.
	The other 3 channels are Worley noise at increasing frequencies.
	This 3d texture is used to define the base shape for our clouds.

	3D cloudDetailsTexture
	3 channels…
	32^3 resolution…
	Uses Worley noise at increasing frequencies.
	This texture is used to add detail to the base cloud shape defined by the first 3d noise.

	2D cloudMotionTexture
	3 channels…
	128^2 resolution…
	Uses curl noise. Which is non divergent and is used to fake fluid motion.
	We use this noise to distort our cloud shapes and add a sense of turbulence.
	*/
	
	void CreateCloudResources(VkCommandPool computeCommandPool);

	VkBuffer GetSunAndSkyBuffer() const;
	void UpdateSunAndSky();
};