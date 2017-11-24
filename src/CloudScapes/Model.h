#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include "VulkanDevice.h"
#include "Vertex.h"
#include "BufferUtils.h"
#include "Image.h"

struct ModelBufferObject {
	glm::mat4 modelMatrix;
};

class Model
{
public:
	Model() = delete;	// https://stackoverflow.com/questions/5513881/meaning-of-delete-after-function-declaration
	Model(VulkanDevice* device, VkCommandPool commandPool, const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices);
	Model(VulkanDevice* device, VkCommandPool commandPool, const std::string model_path, const std::string texture_path);
	~Model();

	void SetTexture(VulkanDevice* device, VkCommandPool commandPool, const std::string texture_path,
					VkImage& textureImage, VkDeviceMemory& textureImageMemory,
					VkImageView& textureImageView, VkSampler& textureSampler);
	void LoadModel(const std::string model_path);

	const std::vector<Vertex>& getVertices() const;
	const std::vector<uint32_t>& getIndices() const;

	uint32_t getVertexBufferSize() const;
	uint32_t getIndexBufferSize() const;

	VkBuffer getVertexBuffer() const;
	VkBuffer getIndexBuffer() const;
	VkBuffer* getPointerToVertexBuffer() const;
	VkBuffer* getPointerToIndexBuffer() const;

	const ModelBufferObject& getModelBufferObject() const;

	VkBuffer GetModelBuffer() const;
	VkImageView GetTextureView() const;
	VkSampler GetTextureSampler() const;

private:
	VulkanDevice* device; //member variable because it is needed for the destructor

	std::vector<Vertex> vertices;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;

	std::vector<uint32_t> indices;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;

	VkBuffer modelBuffer;
	VkDeviceMemory modelBufferMemory;

	ModelBufferObject modelBufferObject;

	VkImage texture = VK_NULL_HANDLE;
	VkDeviceMemory textureMemory = VK_NULL_HANDLE;
	VkImageView textureView = VK_NULL_HANDLE;
	VkSampler textureSampler = VK_NULL_HANDLE;

	void* mappedData;
};