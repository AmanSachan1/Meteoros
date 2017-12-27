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
	Renderer(VulkanDevice* device, VkPhysicalDevice physicalDevice, VulkanSwapChain* swapChain, Scene* scene, Sky* sky, Camera* camera, Camera* cameraOld, uint32_t width, uint32_t height);
	~Renderer();

	void DestroyOnWindowResize();

	void InitializeRenderer();
	void RecreateOnResize(uint32_t width, uint32_t height);

	void Frame();

	void CreateRenderPass();

	// Descriptors
	void CreateDescriptorPool();
	void CreateAllDescriptorSetLayouts();
	void CreateAllDescriptorSets();
	
	void WriteToAndUpdateAllDescriptorSets();
	void WriteToAndUpdateComputeDescriptorSets();
	void WriteToAndUpdateGraphicsDescriptorSets();
	void WriteToAndUpdatePingPongDescriptorSets();
	void WriteToAndUpdateRemainingDescriptorSets();
	void WriteToAndUpdateGodRaysSet();
	void WriteToAndUpdateToneMapSet();
	void WriteToAndUpdateTXAASet();

	// Pipelines
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
	void RecordAllCommandBuffers();
	void RecordComputeCommandBuffer(VkCommandBuffer &computeCmdBuffer, VkDescriptorSet& pingPongFrameSet);
	void RecordGraphicsCommandBuffer(std::vector<VkCommandBuffer> &graphicsCmdBuffer, VkImage &Image_for_barrier, 
									VkDescriptorSet& pingPongFrameSet, VkDescriptorSet& finalPassSet);

	// Resource Creation and Recreation
	void CreateResources();

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
	VkPipelineLayout cloudComputePipelineLayout;
	VkPipelineLayout reprojectionPipelineLayout;
	VkPipeline graphicsPipeline;
	VkPipeline cloudComputePipeline;
	VkPipeline reprojectionPipeline;

	VkPipelineCache postProcessPipeLineCache;
	VkPipelineLayout postProcess_GodRays_PipelineLayout;
	VkPipelineLayout postProcess_ToneMap_PipelineLayout;
	VkPipelineLayout postProcess_TXAA_PipelineLayout;
	VkPipeline postProcess_GodRays_PipeLine;
	VkPipeline postProcess_ToneMap_PipeLine;
	VkPipeline postProcess_TXAA_PipeLine;

	VkRenderPass renderPass;

	std::vector<VkFramebuffer> frameBuffers;
	
	VkImage depthImage;
	VkDeviceMemory depthImageMemory;
	VkImageView depthImageView;

	Texture2D* currentFrameTexture;
	Texture2D* previousFrameTexture;

	Texture2D* currentCloudsResultTexture;
	Texture2D* previousCloudsResultTexture;
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
	VkDescriptorSetLayout cloudComputeSetLayout;	// Compute shader binding layout
	VkDescriptorSetLayout graphicsSetLayout;

	// Descriptor Sets for each pipeline
	VkDescriptorSet cloudComputeSet;	// Compute shader descriptor Set
	VkDescriptorSet graphicsSet; // Graphics ( Regular Geometric Meshes ) specific descriptor sets

	// Descriptor Sets for pingPonged Cloud Results
	VkDescriptorSetLayout pingPongCloudResultSetLayout;
	VkDescriptorSet pingPongCloudResultSet1;
	VkDescriptorSet pingPongCloudResultSet2;

	//Descriptors used in Post Process pipelines
	//God Rays
	VkDescriptorSetLayout godRaysSetLayout;
	VkDescriptorSet godRaysSet;

	//Tone Map
	VkDescriptorSetLayout toneMapSetLayout;
	VkDescriptorSet toneMapSet1;
	VkDescriptorSet toneMapSet2;

	// Descriptor Sets for pingPonged TXAA
	VkDescriptorSetLayout TXAASetLayout;
	VkDescriptorSet TXAASet1;
	VkDescriptorSet TXAASet2;
};