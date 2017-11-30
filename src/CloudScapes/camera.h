#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include "VulkanDevice.h"
#include "BufferUtils.h"

#define PI 3.14159

struct CameraUBO {
	glm::mat4 viewMatrix;
	glm::mat4 invViewMatrix;
	glm::mat4 projectionMatrix;
	glm::vec3 lookAt_worldSpace;
	//float tanFovV;
	//float tanFovH;
	float tanFovVby2;
	float tanFovHby2;
};

class Camera 
{
public:
	Camera() = delete;	// https://stackoverflow.com/questions/5513881/meaning-of-delete-after-function-declaration
	Camera(VulkanDevice* device, VmaAllocator& g_vma_Allocator,
		   glm::vec3 eyePos, glm::vec3 lookatPoint,
		   float foV_vertical, float aspectRatio, float nearClip, float farClip);
	~Camera();

	void UpdateOrbit(float deltaX, float deltaY, float deltaZ);
	void Zoom(float factor);
	VkBuffer GetBuffer() const;

private:
	VulkanDevice* device; //member variable because it is needed for the destructor

	CameraUBO cameraUBO;
	VkBuffer buffer;
	VkDeviceMemory bufferMemory;

	void* mappedData;

	glm::vec3 eyePos;
	glm::vec3 lookAtPos;
	float r, theta, phi;
};