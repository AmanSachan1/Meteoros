#include "camera.h"

Camera::Camera(VulkanDevice* device, glm::vec3 eyePos, glm::vec3 lookAtPoint, int width, int height,
			   float foV_vertical, float aspectRatio, float nearClip, float farClip)
	: device(device), eyePos(eyePos), ref(lookAtPoint), width(width), height(height), 
	fovy(foV_vertical), aspect(aspectRatio), near_clip(nearClip), far_clip(farClip)
{
	worldUp = glm::vec3(0,1,0);
	RecomputeAttributes();
	UpdateBuffer();
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

void Camera::UpdateBuffer()
{
	cameraUBO.view = GetView();
	cameraUBO.proj = GetProj();
	//Reason for flipping the y axis: https://matthewwellings.com/blog/the-new-vulkan-coordinate-system/
	cameraUBO.proj[1][1] *= -1; // y-coordinate is flipped

	cameraUBO.eyePos = eyePos;

	cameraUBO.tanFovVby2 = std::tan(fovy*0.5 * (PI / 180.0));
	cameraUBO.tanFovHby2 = aspect * cameraUBO.tanFovVby2;
}
void Camera::CopyToGPUMemory()
{
	memcpy(mappedData, &cameraUBO, sizeof(CameraUBO));
}

glm::mat4 Camera::GetViewProj() const
{
	//static casts
	return glm::perspective(glm::radians(fovy), width / (float)height, near_clip, far_clip) * glm::lookAt(eyePos, ref, up);
}
glm::mat4 Camera::GetView() const
{
	return glm::lookAt(eyePos, ref, up);
}
glm::mat4 Camera::GetProj() const
{
	return glm::perspective(glm::radians(fovy), width / (float)height, near_clip, far_clip);
}
void Camera::RecomputeAttributes()
{
	forward = glm::normalize(ref - eyePos);
	right = glm::normalize(glm::cross(forward, worldUp));
	up = glm::cross(right, forward);

	float tan_fovy = tan(glm::radians(fovy / 2));
	float len = glm::length(ref - eyePos);
	aspect = width / (float)height;
}

void Camera::RotateAboutUp(float deg)
{
	deg = glm::radians(deg);
	glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), deg, up);
	ref = ref - eyePos;
	ref = glm::vec3(rotation * glm::vec4(ref, 1));
	ref = ref + eyePos;
	RecomputeAttributes();
}
void Camera::RotateAboutRight(float deg)
{
	deg = glm::radians(deg);
	glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), deg, right);
	ref = ref - eyePos;
	ref = glm::vec3(rotation * glm::vec4(ref, 1));
	ref = ref + eyePos;
	RecomputeAttributes();
}

void Camera::TranslateAlongLook(float amt)
{
	glm::vec3 translation = forward * amt;
	eyePos += translation;
	ref += translation;
	RecomputeAttributes();
}
void Camera::TranslateAlongRight(float amt)
{
	glm::vec3 translation = right * amt;
	eyePos += translation;
	ref += translation;
	RecomputeAttributes();
}
void Camera::TranslateAlongUp(float amt)
{
	glm::vec3 translation = up * amt;
	eyePos += translation;
	ref += translation;
	RecomputeAttributes();
}