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
#include "Sky.h"
#include "FormatUtils.h"

static constexpr unsigned int WORKGROUP_SIZE = 32;

class Renderer 
{
public:
	Renderer() = delete; // To enforce the creation of a the type of renderer we want without leaving the vulkan device, vulkan swapchain, etc as assumptions or nullptrs
	Renderer(VulkanDevice* device, VkPhysicalDevice physicalDevice, VulkanSwapChain* swapChain, Scene* scene, Sky* sky, Camera* camera, uint32_t width, uint32_t height);
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
	void WriteToAndUpdatePingPongDescriptorSets();
	void WriteToAndUpdateRemainingDescriptorSets();
	void WriteToAndUpdatePostDescriptorSets();

	// Pipelines
	VkPipelineLayout CreatePipelineLayout(std::vector<VkDescriptorSetLayout> descriptorSetLayouts);
	void CreateAllPipeLines(VkRenderPass renderPass, unsigned int subpass);
	void CreateGraphicsPipeline(VkRenderPass renderPass, unsigned int subpass);
	void CreateComputePipeline(VkPipelineLayout& _computePipelineLayout, VkPipeline& _computePipeline, const std::string &filename);
	void CreatePostProcessPipeLines(VkRenderPass renderPass);

	// Frame Resources
	void CreateFrameResources();
	void DestroyFrameResources();
	void RecreateFrameResources();
	void CreateFrameBuffers(VkRenderPass renderPass);

	// Command Buffers
	void RecordGraphicsCommandBuffer(std::vector<VkCommandBuffer> &graphicsCmdBuffer, VkImage &Image_for_barrier, 
		VkDescriptorSet& pingPongFrameSet, VkDescriptorSet& finalPassSet);
	void RecordComputeCommandBuffer(VkCommandBuffer &computeCmdBuffer, VkDescriptorSet& pingPongFrameSet);

	// Resource Creation and Recreation
	void CreateComputeResources();
	void RecreateComputeResources();
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
	Sky* sky;

	uint32_t window_width;
	uint32_t window_height;

	// We create a vector of command buffers because we want a command buffer for each frame of the swap chain
	std::vector<VkCommandBuffer> graphicsCommandBuffer1;
	VkCommandBuffer computeCommandBuffer1;
	std::vector<VkCommandBuffer> graphicsCommandBuffer2;
	VkCommandBuffer computeCommandBuffer2;
	VkCommandPool graphicsCommandPool;
	VkCommandPool computeCommandPool;

	VkPipelineLayout graphicsPipelineLayout;
	VkPipelineLayout computePipelineLayout;
	VkPipelineLayout reprojectionPipelineLayout;
	VkPipeline graphicsPipeline;
	VkPipeline computePipeline;
	VkPipeline reprojectionPipeline;

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
	VkDescriptorSetLayout pingPongFrameSetLayout;	// Compute shader binding layout

	// Descriptor Sets for each pipeline
	VkDescriptorSet computeSet;	// Compute shader descriptor Set
	VkDescriptorSet graphicsSet; // Graphics ( Regular Geometric Meshes ) specific descriptor sets
	VkDescriptorSet pingPongFrameSet1;	// Compute shader descriptor Set
	VkDescriptorSet pingPongFrameSet2;	// Compute shader descriptor Set

	//Descriptors used in Post Process pipelines
	VkDescriptorSetLayout godRaysSetLayout;
	VkDescriptorSet godRaysSet;

	VkDescriptorSetLayout finalPassSetLayout;
	VkDescriptorSet finalPassSet1;
	VkDescriptorSet finalPassSet2;

	VkDescriptorSetLayout storageImagePingPongSetLayout;
	VkDescriptorSet storageImagePingPongSet1;
	VkDescriptorSet storageImagePingPongSet2;
};