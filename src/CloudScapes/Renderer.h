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
#include "Texture2D.h"

static constexpr unsigned int WORKGROUP_SIZE = 32;

class Renderer 
{
public:
	Renderer() = delete; // To enforce the creation of a the type of renderer we want without leaving the vulkan device, vulkan swapchain, etc as assumptions or nullptrs
	Renderer(VulkanDevice* device, VkPhysicalDevice physicalDevice, VulkanSwapChain* swapChain, Scene* scene, Camera* camera, uint32_t width, uint32_t height);
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
	void CreateCloudsPipeline(VkRenderPass renderPass, unsigned int subpass);
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
	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
								VkImageTiling tiling, VkFormatFeatureFlags features);
	VkFormat findDepthFormat();

private:
	VulkanDevice* device; // manages both the logical device (VkDevice) and the physical Device (VkPhysicalDevice)
	VkDevice logicalDevice;
	VkPhysicalDevice physicalDevice;

	VulkanSwapChain* swapChain;

	Camera* camera;
	Scene* scene;

	uint32_t window_width;
	uint32_t window_height;

	// We create a vector of command buffers because we want a command buffer for each frame of the swap chain
	std::vector<VkCommandBuffer> graphicsCommandBuffer;
	VkCommandBuffer computeCommandBuffer;
	VkCommandPool commandPool; // can use the same command pool for both types of command buffers

	VkPipelineLayout cloudsPipelineLayout;
	VkPipelineLayout graphicsPipelineLayout;
	VkPipelineLayout computePipelineLayout;
	VkPipeline cloudsPipeline;
	VkPipeline graphicsPipeline;
	VkPipeline computePipeline;

	VkRenderPass renderPass;

	std::vector<VkImageView> imageViews;
	std::vector<VkFramebuffer> frameBuffers;
	
	// Change the buffers when you set it up in a models class
	Model* house;
	Model* quad;

	VkImage depthImage;
	VkDeviceMemory depthImageMemory;
	VkImageView depthImageView;

	Texture2D* rayMarchedComputeTexture;

	VkDescriptorPool descriptorPool;

	VkDescriptorSetLayout cameraSetLayout;
	VkDescriptorSetLayout modelSetLayout;
	VkDescriptorSetLayout samplerSetLayout;

	// Cloud Pipeline Descriptor Sets + compute pipeline descriptor Set Layout
	VkDescriptorSetLayout cloudPreComputeSetLayout;	// Compute shader binding layout
	VkDescriptorSet cloudPreComputeSet;			// Compute shader bindings
	VkDescriptorSetLayout cloudPostComputeSetLayout;
	VkDescriptorSet cloudPostComputeSet;
	
	// Geometry specific descriptor sets
	VkDescriptorSet cameraSet;
	VkDescriptorSet geomModelSet;
	VkDescriptorSet geomSamplerSet;
};