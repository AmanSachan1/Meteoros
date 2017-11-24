#include "camera.h"

Camera::Camera(VulkanDevice* device,
			   glm::vec3 eyePos, glm::vec3 lookAtPoint,
			   float foV_vertical, float aspectRatio, float nearClip, float farClip)
	: device(device), eyePos(eyePos), lookAtPos(lookAtPoint)
{
	r = eyePos.z;
	theta = 0.0f;
	phi = 0.0f;
	cameraUBO.viewMatrix = glm::lookAt(eyePos, lookAtPos, glm::vec3(0.0f, 1.0f, 0.0f));
	cameraUBO.projectionMatrix = glm::perspective(static_cast<float>(foV_vertical * M_PI / 180), aspectRatio, nearClip, farClip);
	//Reason for flipping the y axis: https://matthewwellings.com/blog/the-new-vulkan-coordinate-system/
	cameraUBO.projectionMatrix[1][1] *= -1; // y-coordinate is flipped

	BufferUtils::CreateBuffer(device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(CameraUBO), 
							VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
							buffer, bufferMemory);

	vkMapMemory(device->GetVkDevice(), bufferMemory, 0, sizeof(CameraUBO), 0, &mappedData);
	memcpy(mappedData, &cameraUBO, sizeof(CameraUBO));
}

Camera::~Camera() 
{
	vkUnmapMemory(device->GetVkDevice(), bufferMemory);
	vkDestroyBuffer(device->GetVkDevice(), buffer, nullptr);
	vkFreeMemory(device->GetVkDevice(), bufferMemory, nullptr);
}

VkBuffer Camera::GetBuffer() const 
{
	return buffer;
}

void Camera::UpdateOrbit(float deltaX, float deltaY, float deltaZ) 
{
	theta += deltaX;
	phi += deltaY;
	r = glm::clamp(r - deltaZ, 1.0f, 500.0f);

	float radTheta = glm::radians(theta);
	float radPhi = glm::radians(phi);

	glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), radTheta, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::rotate(glm::mat4(1.0f), radPhi, glm::vec3(1.0f, 0.0f, 0.0f));
	glm::mat4 finalTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f)) * rotation * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, r));

	cameraUBO.viewMatrix = glm::inverse(finalTransform);

	memcpy(mappedData, &cameraUBO, sizeof(CameraUBO));
}

void Camera::Zoom(float factor)
{
	r *= glm::exp(-factor);

	float radTheta = glm::radians(theta);
	float radPhi = glm::radians(phi);

	glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), radTheta, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::rotate(glm::mat4(1.0f), radPhi, glm::vec3(1.0f, 0.0f, 0.0f));
	glm::mat4 finalTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f)) * rotation * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, r));

	cameraUBO.viewMatrix = glm::inverse(finalTransform);

	memcpy(mappedData, &cameraUBO, sizeof(CameraUBO));
}