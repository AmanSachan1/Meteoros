#pragma once

#include <glm/glm.hpp>
#include <chrono>

#include "VulkanDevice.h"
#include "BufferUtils.h"
#include "Model.h"

using namespace std::chrono;

struct Time 
{
	float deltaTime = 0.0f;
	float totalTime = 0.0f;
};

class Scene 
{
private:
	VulkanDevice* device;

	Time time;
	VkBuffer timeBuffer;
	VkDeviceMemory timeBufferMemory;
	void* mappedData;

	std::vector<Model*> models;

	high_resolution_clock::time_point startTime = high_resolution_clock::now();

public:
	Scene() = delete;
	Scene(VulkanDevice* device);
	~Scene();

	const std::vector<Model*>& GetModels() const;
	void AddModel(Model* model);

	VkBuffer GetTimeBuffer() const;
	void UpdateTime();
};