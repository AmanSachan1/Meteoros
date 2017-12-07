#pragma once

#include <glm/glm.hpp>
#include <chrono>

#include "VulkanDevice.h"
#include "BufferUtils.h"
#include "Model.h"

using namespace std::chrono;

struct Time 
{
	glm::vec2 _time = glm::vec2(0.0f, 0.0f); //stores delta time and total time packed as a vec2 so vulkan offsetting doesnt become an issue later
};

struct SunAndSky
{
	glm::vec4 sunLocation = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
};

struct KeyPressQuery
{
	int key_debug = 0;
};

class Scene 
{
private:
	VulkanDevice* device;

	Time time;
	VkBuffer timeBuffer;
	VkDeviceMemory timeBufferMemory;
	void* time_mappedData;

	SunAndSky sunAndSky;
	VkBuffer sunAndSkyBuffer;
	VkDeviceMemory sunAndSkyBufferMemory;
	void* sunAndSky_mappedData;

	KeyPressQuery keyPressQuery;
	VkBuffer keyPressQueryBuffer;
	VkDeviceMemory keyPressQueryBufferMemory;
	void* keyPressQuery_mappedData;

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
	glm::vec2 GetTime() const;

	VkBuffer GetSunAndSkyBuffer() const;
	void UpdateSunAndSky();
	//SunAndSky GetSunAndSky() const;

	VkBuffer GetKeyPressQueryBuffer() const;
	void UpdateKeyPressQuery();
	//KeyPressQuery GetKeyPressQuery() const;
};