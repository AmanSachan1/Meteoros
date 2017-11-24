#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <iostream>

#include "VulkanDevice.h"
#include "SwapChain.h"
#include "ShaderModule.h"
#include "BufferUtils.h"
#include "Vertex.h"
#include "Image.h"
#include "Camera.h"
#include "Scene.h"
#include "Model.h"

static constexpr unsigned int WORKGROUP_SIZE = 32;

class Renderer 
{
public:
	Renderer() = delete; // To enforce the creation of a the type of renderer we want without leaving the vulkan device, vulkan swapchain, etc as assumptions or nullptrs
	Renderer(VulkanDevice* device, VulkanSwapChain* swapChain, Scene* scene, Camera* camera);
	~Renderer();

	void InitializeRenderer();

	void Frame();

	void CreateCommandPools();
	void CreateRenderPass();

	// Descriptor Pool
	void CreateDescriptorPool();

	// Descriptor SetLayouts
	void CreateAllDescriptorSetLayouts();
	VkDescriptorSetLayout CreateDescriptorSetLayout(std::vector<VkDescriptorSetLayoutBinding> layoutBindings);

	// Descriptor Sets
	void CreateAllDescriptorSets();
	VkDescriptorSet CreateDescriptorSet(VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout);

	// Helper Functions for Creating DescriptorSets
	void CreateCloudTextureResources(VkImage& textureImage, VkDeviceMemory& textureImageMemory, VkImageView& textureImageView, VkSampler& textureSampler);
	void WriteToAndUpdateDescriptorSets();

	// Pipelines
	VkPipelineLayout CreatePipelineLayout(std::vector<VkDescriptorSetLayout> descriptorSetLayouts);
	void CreateGraphicsPipeline(VkRenderPass renderPass, unsigned int subpass);
	void CreateComputePipeline();

	// Frame Resources
	void CreateFrameResources();
	void DestroyFrameResources();
	void RecreateFrameResources();

	// Helper Functions for Frame Resources
	void CreateImageViewsforFrame();
	void createDepthResources();
	void CreateFrameBuffers(VkRenderPass renderPass);

	// Command Buffers
	void RecordGraphicsCommandBuffer();
	void RecordComputeCommandBuffer();

	// Format Helper Functions
	VkFormat findSupportedFormat(VkPhysicalDevice physicalDevice,
								const std::vector<VkFormat>& candidates,
								VkImageTiling tiling, VkFormatFeatureFlags features);
	VkFormat findDepthFormat();

private:
	VulkanDevice* device; // manages both the logical device (VkDevice) and the physical Device (VkPhysicalDevice)
	VkDevice logicalDevice;

	VulkanSwapChain* swapChain;

	Camera* camera;
	Scene* scene;

	// We create a vector of command buffers because we want a command buffer for each frame of the swap chain
	std::vector<VkCommandBuffer> graphicsCommandBuffer;
	std::vector<VkCommandBuffer> computeCommandBuffer;
	VkCommandPool graphicsCommandPool;
	VkCommandPool computeCommandPool;

	VkPipelineLayout graphicsPipelineLayout;
	VkPipelineLayout computePipelineLayout;
	VkPipeline graphicsPipeline;
	VkPipeline computePipeline;

	VkRenderPass renderPass;

	std::vector<VkImageView> imageViews;
	std::vector<VkFramebuffer> frameBuffers;
	
	// Change the buffers when you set it up in a models class
	Model* house;

	VkImage houseTextureImage;
	VkDeviceMemory houseTextureImageMemory;
	VkImageView houseTextureImageView;
	VkSampler houseTextureSampler;

	VkImage textureImage;
	VkDeviceMemory textureImageMemory;
	VkImageView textureImageView;
	VkSampler textureSampler;

	VkImage depthImage;
	VkDeviceMemory depthImageMemory;
	VkImageView depthImageView;

	VkDescriptorPool descriptorPool;
	VkDescriptorSetLayout computeSetLayout;
	VkDescriptorSetLayout cameraSetLayout;
	VkDescriptorSetLayout modelSetLayout;
	VkDescriptorSetLayout samplerSetLayout;
	VkDescriptorSet computeSet;
	VkDescriptorSet cameraSet;
	VkDescriptorSet modelSet;
	VkDescriptorSet samplerSet;
};