#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <iostream>

#include "VulkanDevice.h"
#include "VulkanInitializers.h"
#include "SwapChain.h"
#include "ShaderModule.h"
#include "BufferUtils.h"
#include "Vertex.h"
#include "Image.h"
#include "Camera.h"
#include "Scene.h"
#include "Model.h"
#include "Texture2D.h"
#include "Texture3D.h"
#include "FormatUtils.h"

static constexpr unsigned int WORKGROUP_SIZE = 32;

class Renderer 
{
public:
	Renderer() = delete; // To enforce the creation of a the type of renderer we want without leaving the vulkan device, vulkan swapchain, etc as assumptions or nullptrs
	Renderer(VulkanDevice* device, VkPhysicalDevice physicalDevice, VulkanSwapChain* swapChain, Scene* scene, Camera* camera, uint32_t width, uint32_t height);
	~Renderer();

	void DestroyOnWindowResize();

	void InitializeRenderer();
	void RecreateOnResize(uint32_t width, uint32_t height);

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
	
	void WriteToAndUpdateAllDescriptorSets();
	// Helper Functions forWriting and updating Descriptor Sets
	void WriteToAndUpdateComputeDescriptorSets();
	void WriteToAndUpdateGraphicsDescriptorSets();
	void WriteToAndUpdateRemainingDescriptorSets();
	void WriteToAndUpdatePostDescriptorSets();

	// Pipelines
	VkPipelineLayout CreatePipelineLayout(std::vector<VkDescriptorSetLayout> descriptorSetLayouts);
	void CreateAllPipeLines(VkRenderPass renderPass, unsigned int subpass);
	void CreateGraphicsPipeline(VkRenderPass renderPass, unsigned int subpass);
	void CreateComputePipeline();	
	void CreatePostProcessPipeLines(VkRenderPass renderPass);

	// Frame Resources
	void CreateFrameResources();
	void DestroyFrameResources();
	void RecreateFrameResources();

	// Helper Functions for Frame Resources
	void CreateDepthResources();
	void CreateFrameBuffers(VkRenderPass renderPass);

	// Command Buffers
	void RecordGraphicsCommandBuffer();
	void RecordComputeCommandBuffer();

	// Resource Creation and Recreation
	void CreateComputeResources();
	void RecreateComputeResources();
	void CreateCloudResources();
	void CreatePostProcessResources();

private:
	VulkanDevice* device; // manages both the logical device (VkDevice) and the physical Device (VkPhysicalDevice)

	VkDevice logicalDevice;
	VkPhysicalDevice physicalDevice;
	VulkanSwapChain* swapChain;
	bool swapPingPongBuffers = false;

	Camera* camera;
	Camera* cameraOld;
	Scene* scene;

	uint32_t window_width;
	uint32_t window_height;

	// We create a vector of command buffers because we want a command buffer for each frame of the swap chain
	std::vector<VkCommandBuffer> graphicsCommandBuffer;
	VkCommandBuffer computeCommandBuffer;
	VkCommandPool graphicsCommandPool;
	VkCommandPool computeCommandPool;

	VkPipelineLayout graphicsPipelineLayout;
	VkPipelineLayout computePipelineLayout;
	VkPipelineLayout reprojectionComputePipelineLayout;
	VkPipeline graphicsPipeline;
	VkPipeline computePipeline;
	VkPipelineLayout reprojectionComputePipeline;

	VkPipelineCache postProcessPipeLineCache;
	VkPipelineLayout postProcess_GodRays_PipelineLayout;
	VkPipeline postProcess_GodRays_PipeLine;

	VkPipelineLayout postProcess_FinalPass_PipelineLayout;
	VkPipeline postProcess_FinalPass_PipeLine;

	VkRenderPass renderPass;

	std::vector<VkFramebuffer> frameBuffers;
	
	VkImage depthImage;
	VkDeviceMemory depthImageMemory;
	VkImageView depthImageView;

	Texture2D* currentFrameResultTexture;
	Texture2D* previousFrameComputeResultTexture;
	Texture2D* godRaysCreationDataTexture;

	Texture2D* weatherMapTexture;
	Texture3D* cloudBaseShapeTexture;
	Texture3D* cloudDetailsTexture;
	Texture2D* cloudMotionTexture;
	/*
		3D cloudBaseShapeTexture
		4 channels…
		128^3 resolution…
		The first channel is the Perlin-Worley noise.
		The other 3 channels are Worley noise at increasing frequencies. 
		This 3d texture is used to define the base shape for our clouds.

		3D cloudDetailsTexture
		3 channels…
		32^3 resolution…
		Uses Worley noise at increasing frequencies. 
		This texture is used to add detail to the base cloud shape defined by the first 3d noise.

		2D cloudMotionTexture
		3 channels…
		128^2 resolution…
		Uses curl noise. Which is non divergent and is used to fake fluid motion. 
		We use this noise to distort our cloud shapes and add a sense of turbulence.
	*/
	
	VkDescriptorPool descriptorPool;

	//Descriptors used in multiple Pipelines
	VkDescriptorSetLayout cameraSetLayout;
	VkDescriptorSet cameraSet;
	VkDescriptorSet cameraOldSet;
	VkDescriptorSetLayout timeSetLayout;
	VkDescriptorSet timeSet;
	VkDescriptorSetLayout sunAndSkySetLayout;
	VkDescriptorSet sunAndSkySet;
	VkDescriptorSetLayout keyPressQuerySetLayout;
	VkDescriptorSet keyPressQuerySet;

	//Descriptor Set Layouts for each pipeline
	VkDescriptorSetLayout computeSetLayout;	// Compute shader binding layout
	VkDescriptorSetLayout graphicsSetLayout;

	// Descriptor Sets for each pipeline
	VkDescriptorSet computeSet;	// Compute shader descriptor Set
	VkDescriptorSet graphicsSet; // Graphics ( Regular Geometric Meshes ) specific descriptor sets

	//Descriptors used in Post Process pipelines
	VkDescriptorSetLayout godRaysSetLayout;
	VkDescriptorSet godRaysSet;

	VkDescriptorSetLayout finalPassSetLayout;
	VkDescriptorSet finalPassSet;

	VkDescriptorSetLayout storageImagePingPongSetLayout;
	VkDescriptorSet storageImagePingPongSet1;
	VkDescriptorSet storageImagePingPongSet2;
};