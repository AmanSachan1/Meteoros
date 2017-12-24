#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include "VulkanDevice.h"
#include "BufferUtils.h"

#define PI 3.14159

struct CameraUBO {
	glm::mat4 view;
	glm::mat4 proj;
	glm::vec4 eyePos; //pad vec3s with extra float to make them vec4s so vulkan can do offsets correctly
	glm::vec2 tanFovBy2; //vec2 and vec4 are acceptable for offseting; 
	//stored as .x = horizontalFovBy2 and .y = verticalFovBy2
};

class Camera 
{
public:
	Camera() = delete;	// https://stackoverflow.com/questions/5513881/meaning-of-delete-after-function-declaration
	Camera(VulkanDevice* device, glm::vec3 eyePos, glm::vec3 ref, int width, int height,
		   float foV_vertical, float aspectRatio, float nearClip, float farClip);
	~Camera();

	VkBuffer GetBuffer() const;
	void UpdateBuffer();
	void UpdateBuffer(Camera* cam);
	void CopyToGPUMemory();

	glm::mat4 GetView() const;
	glm::mat4 GetProj() const;
	glm::mat4 GetViewProj() const;
	void RecomputeAttributes();

	void RotateAboutUp(float deg);
	void RotateAboutRight(float deg);

	void TranslateAlongLook(float amt);
	void TranslateAlongRight(float amt);
	void TranslateAlongUp(float amt);

private:
	VulkanDevice* device; //member variable because it is needed for the destructor

	CameraUBO cameraUBO;
	VkBuffer buffer;
	VkDeviceMemory bufferMemory;

	void* mappedData;

	glm::vec3 eyePos;
	glm::vec3 ref;      //The point in world space towards which the camera is pointing

	glm::vec3 forward;
	glm::vec3 right;
	glm::vec3 up;
	glm::vec3 worldUp;

	int width, height;

	float fovy;
	float aspect;
	float near_clip;  // Near clip plane distance
	float far_clip;  // Far clip plane distance
};