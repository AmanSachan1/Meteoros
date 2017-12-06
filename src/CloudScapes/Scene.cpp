#include "Scene.h"

Scene::Scene(VulkanDevice* device) : device(device) 
{
	BufferUtils::CreateBuffer(device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(Time), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, timeBuffer, timeBufferMemory);
	vkMapMemory(device->GetVkDevice(), timeBufferMemory, 0, sizeof(Time), 0, &mappedData);
	memcpy(mappedData, &time, sizeof(Time));
}

Scene::~Scene()
{
	vkUnmapMemory(device->GetVkDevice(), timeBufferMemory);
	vkDestroyBuffer(device->GetVkDevice(), timeBuffer, nullptr);
	vkFreeMemory(device->GetVkDevice(), timeBufferMemory, nullptr);
}

const std::vector<Model*>& Scene::GetModels() const
{
	return models;
}
void Scene::AddModel(Model* model)
{
	models.push_back(model);
}

VkBuffer Scene::GetTimeBuffer() const
{
	return timeBuffer;
}
void Scene::UpdateTime()
{
	high_resolution_clock::time_point currentTime = high_resolution_clock::now();
	duration<float> nextDeltaTime = duration_cast<duration<float>>(currentTime - startTime);
	startTime = currentTime;

	time._time.x = nextDeltaTime.count();
	time._time.y += time._time.x;

	memcpy(mappedData, &time, sizeof(Time));
}

glm::vec2 Scene::GetTime() const
{
	return time._time;
}