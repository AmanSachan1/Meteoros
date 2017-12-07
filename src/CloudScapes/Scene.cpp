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

void Scene::CreateModelsInScene(VkCommandPool commandPool)
{
	// Model and texture file paths
	const std::string model_path = "../../src/CloudScapes/models/teapot.obj";
	//const std::string model_path = "../../src/CloudScapes/models/chaletModel.obj";
	//const std::string model_path = "../../src/CloudScapes/models/cyllinder2.obj";
	//const std::string model_path = "../../src/CloudScapes/models/wahoo.obj";
	//const std::string model_path = "../../src/CloudScapes/models/dodecahedron.obj";

	const std::string texture_path = "../../src/CloudScapes/textures/statue.jpg";

	// Using .obj-based Model constructor ----------------------------------------------------
	//house = new Model(device, commandPool, g_vma_Allocator, model_path, texture_path);

	// Using manual-based Model constructor --------------------------------------------------
	const std::vector<Vertex> vertices = {
		{ { 1.0f, 0.0f, -1.0f,  1.0f },{ 1.0f, 0.0f, 0.0f, 1.0f },{ 1.0f, 0.0f } },
		{ { -1.0f,  0.0f, 1.0f,  1.0f },{ 0.0f, 1.0f, 0.0f, 1.0f },{ 0.0f, 0.0f } },
		{ { -1.0f,  0.0f, 1.0f, 1.0f },{ 0.0f, 0.0f, 1.0f, 1.0f },{ 0.0f, 1.0f } },
		{ { 1.0f, 0.0f, -1.0f, 1.0f },{ 1.0f, 1.0f, 1.0f, 1.0f },{ 1.0f, 1.0f } },
	};
	std::vector<unsigned int> indices = { 0, 1, 2, 2, 3, 0 };//2,1,0,0,2,3};
	Model* GroundPlane = new Model(device, commandPool, vertices, indices);
	GroundPlane->SetTexture(device, commandPool, texture_path);

	//glm::mat4 modelMat = glm::rotate(house->GetModelMatrix(), scene->GetTime().y * glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 1.0f)); //Ask Austin why the rotation doesnt work
	//GroundPlane->SetModelBuffer(modelMat);

	AddModel(GroundPlane);
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

