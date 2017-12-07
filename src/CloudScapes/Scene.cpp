#include "Scene.h"

Scene::Scene(VulkanDevice* device) : device(device) 
{
	BufferUtils::CreateBuffer(device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(Time), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, timeBuffer, timeBufferMemory);
	vkMapMemory(device->GetVkDevice(), timeBufferMemory, 0, sizeof(Time), 0, &time_mappedData);
	memcpy(time_mappedData, &time, sizeof(Time));

	BufferUtils::CreateBuffer(device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(SunAndSky), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sunAndSkyBuffer, sunAndSkyBufferMemory);
	vkMapMemory(device->GetVkDevice(), sunAndSkyBufferMemory, 0, sizeof(SunAndSky), 0, &sunAndSky_mappedData);
	memcpy(sunAndSky_mappedData, &sunAndSky, sizeof(SunAndSky));

	BufferUtils::CreateBuffer(device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(KeyPressQuery), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, keyPressQueryBuffer, keyPressQueryBufferMemory);
	vkMapMemory(device->GetVkDevice(), keyPressQueryBufferMemory, 0, sizeof(KeyPressQuery), 0, &keyPressQuery_mappedData);
	memcpy(keyPressQuery_mappedData, &keyPressQuery, sizeof(KeyPressQuery));
}

Scene::~Scene()
{
	vkUnmapMemory(device->GetVkDevice(), timeBufferMemory);
	vkDestroyBuffer(device->GetVkDevice(), timeBuffer, nullptr);
	vkFreeMemory(device->GetVkDevice(), timeBufferMemory, nullptr);

	vkUnmapMemory(device->GetVkDevice(), sunAndSkyBufferMemory);
	vkDestroyBuffer(device->GetVkDevice(), sunAndSkyBuffer, nullptr);
	vkFreeMemory(device->GetVkDevice(), sunAndSkyBufferMemory, nullptr);

	vkUnmapMemory(device->GetVkDevice(), keyPressQueryBufferMemory);
	vkDestroyBuffer(device->GetVkDevice(), keyPressQueryBuffer, nullptr);
	vkFreeMemory(device->GetVkDevice(), keyPressQueryBufferMemory, nullptr);
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

	memcpy(time_mappedData, &time, sizeof(Time));
}
glm::vec2 Scene::GetTime() const
{
	return time._time;
}

VkBuffer Scene::GetSunAndSkyBuffer() const
{
	return sunAndSkyBuffer;
}
void Scene::UpdateSunAndSky()
{
	memcpy(sunAndSky_mappedData, &sunAndSky, sizeof(SunAndSky));
}

VkBuffer Scene::GetKeyPressQueryBuffer() const
{
	return keyPressQueryBuffer;
}
void Scene::UpdateKeyPressQuery()
{
	memcpy(keyPressQuery_mappedData, &keyPressQuery, sizeof(KeyPressQuery));
}

