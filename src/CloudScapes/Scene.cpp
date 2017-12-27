#include "Scene.h"

#define PI_BY_2 1.57f

Scene::Scene(VulkanDevice* device) : device(device) 
{
	BufferUtils::CreateBuffer(device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(Time), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, timeBuffer, timeBufferMemory);
	vkMapMemory(device->GetVkDevice(), timeBufferMemory, 0, sizeof(Time), 0, &time_mappedData);
	memcpy(time_mappedData, &time, sizeof(Time));

	InitializeTime();

	BufferUtils::CreateBuffer(device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(KeyPressQuery), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, keyPressQueryBuffer, keyPressQueryBufferMemory);
	vkMapMemory(device->GetVkDevice(), keyPressQueryBufferMemory, 0, sizeof(KeyPressQuery), 0, &keyPressQuery_mappedData);
	memcpy(keyPressQuery_mappedData, &keyPressQuery, sizeof(KeyPressQuery));
}

Scene::~Scene()
{
	vkUnmapMemory(device->GetVkDevice(), timeBufferMemory);
	vkDestroyBuffer(device->GetVkDevice(), timeBuffer, nullptr);
	vkFreeMemory(device->GetVkDevice(), timeBufferMemory, nullptr);

	vkUnmapMemory(device->GetVkDevice(), keyPressQueryBufferMemory);
	vkDestroyBuffer(device->GetVkDevice(), keyPressQueryBuffer, nullptr);
	vkFreeMemory(device->GetVkDevice(), keyPressQueryBufferMemory, nullptr);

	for (int i = 0; i < models.size(); i++)
	{
		delete models[i];
	}

	models.erase(models.begin(), models.end());
}

void Scene::CreateModelsInScene(VkCommandPool commandPool)
{
	// TODO: Large Models cause a crash; Not entirely sure why.
	// Model and texture file paths
	const std::string model_path = "../../src/CloudScapes/models/thinCube.obj";
	const std::string texture_path = "../../src/CloudScapes/textures/DarkPavement.png";

	// Using .obj-based Model constructor ----------------------------------------------------
	Model* groundPlane = new Model(device, commandPool, model_path, texture_path);
	
	glm::mat4 modelMat = groundPlane->GetModelMatrix();
	modelMat = glm::translate(modelMat, glm::vec3(0.0f, -0.5f, 0.0f)) * glm::scale(modelMat, glm::vec3(10.0f, 1.0f, 10.0f)) * modelMat;
	groundPlane->SetModelBuffer(modelMat);

	AddModel(groundPlane);
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

	time.frameCount += 1;

	memcpy(time_mappedData, &time, sizeof(Time));
}
void Scene::InitializeTime()
{
	high_resolution_clock::time_point currentTime = high_resolution_clock::now();
	duration<float> nextDeltaTime = duration_cast<duration<float>>(currentTime - startTime);
	startTime = currentTime;

	time._time.x = nextDeltaTime.count();
	time._time.y += time._time.x;

	//generate 8 numbers from the halton sequence for TXAA
	time.haltonSeq1.x = HaltonSequenceAt(1, 3);
	time.haltonSeq1.y = HaltonSequenceAt(2, 3);
	time.haltonSeq1.z = HaltonSequenceAt(3, 3);
	time.haltonSeq1.w = HaltonSequenceAt(4, 3);

	time.haltonSeq2.x = HaltonSequenceAt(5, 3);
	time.haltonSeq2.y = HaltonSequenceAt(6, 3);
	time.haltonSeq2.z = HaltonSequenceAt(7, 3);
	time.haltonSeq2.w = HaltonSequenceAt(8, 3);

	time.haltonSeq3.x = HaltonSequenceAt(9, 3);
	time.haltonSeq3.y = HaltonSequenceAt(10, 3);
	time.haltonSeq3.z = HaltonSequenceAt(11, 3);
	time.haltonSeq3.w = HaltonSequenceAt(12, 3);

	time.haltonSeq4.x = HaltonSequenceAt(13, 3);
	time.haltonSeq4.y = HaltonSequenceAt(14, 3);
	time.haltonSeq4.z = HaltonSequenceAt(15, 3);
	time.haltonSeq4.w = HaltonSequenceAt(16, 3);

	time.frameCount = 0;

	memcpy(time_mappedData, &time, sizeof(Time));
}
glm::vec2 Scene::GetTime() const
{
	return time._time;
}
//Reference: https://en.wikipedia.org/wiki/Halton_sequence
float Scene::HaltonSequenceAt(int index, int base)
{
	float f = 1.0f;
	float r = 0.0f;

	while (index > 0)
	{
		f = f / float(base);
		r += f*(index%base);
		index = int(std::floor(index / base));
	}

	return r;
}

VkBuffer Scene::GetKeyPressQueryBuffer() const
{
	return keyPressQueryBuffer;
}
void Scene::UpdateKeyPressQuery()
{
	memcpy(keyPressQuery_mappedData, &keyPressQuery, sizeof(KeyPressQuery));
}