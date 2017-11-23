#pragma once

#include <glm/glm.hpp>
#include <chrono>

#include "BufferUtils.h"
#include "VulkanDevice.h"

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
	VkBuffer timeBuffer;
	VkDeviceMemory timeBufferMemory;
	Time time;

	high_resolution_clock::time_point startTime = high_resolution_clock::now();

public:
	Scene() = delete;
	Scene(VulkanDevice* device);
	~Scene();

	VkBuffer GetTimeBuffer() const;

	void UpdateTime();
};