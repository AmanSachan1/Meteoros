#pragma once

#include <glm/glm.hpp>
#include <chrono>

#include "VulkanDevice.h"
#include "BufferUtils.h"
#include "Model.h"

using namespace std::chrono;

struct Time 
{
	glm::vec4 haltonSeq1;
	glm::vec4 haltonSeq2;
	glm::vec2 _time = glm::vec2(0.0f, 0.0f); //stores delta time and total time packed as a vec2 so vulkan offsetting doesnt become an issue later
	int frameCount = 1;
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

	void CreateModelsInScene(VkCommandPool commandPool);
	const std::vector<Model*>& GetModels() const;
	void AddModel(Model* model);

	VkBuffer GetTimeBuffer() const;
	void UpdateTime();
	void InitializeTime();
	glm::vec2 GetTime() const;
	float HaltonSequenceAt(int index, int base);

	VkBuffer GetKeyPressQueryBuffer() const;
	void UpdateKeyPressQuery();
	//KeyPressQuery GetKeyPressQuery() const;
};