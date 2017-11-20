#pragma once

#include <stdexcept>
#include <vulkan/vulkan.h>
#include "window.h"
#include "vulkan_instance.h"
#include "vulkan_shader_module.h"
#include "BufferUtils.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <camera.h>
#include <iostream>

#include "vulkan_texturemapping.h"

VulkanInstance* instance;
VulkanDevice* device; // manages both the logical device (VkDevice) and the physical Device (VkPhysicalDevice)
VulkanSwapChain* swapchain; 

Camera* camera;
glm::mat4* mappedCameraView;

namespace {
	bool buttons[GLFW_MOUSE_BUTTON_LAST + 1] = { 0 };

	void mouseButtonCallback(GLFWwindow*, int button, int action, int) {
		buttons[button] = (action == GLFW_PRESS);
	}

	void cursorPosCallback(GLFWwindow*, double mouseX, double mouseY) {
		static double oldX, oldY;
		float dX = static_cast<float>(mouseX - oldX);
		float dY = static_cast<float>(mouseY - oldY);
		oldX = mouseX;
		oldY = mouseY;

		if (buttons[2] || (buttons[0] && buttons[1])) {
			camera->pan(-dX * 0.002f, dY * -0.002f);
			memcpy(mappedCameraView, &camera->view(), sizeof(glm::mat4));
		}
		else if (buttons[0]) {
			camera->rotate(dX * -0.01f, dY * -0.01f);
			memcpy(mappedCameraView, &camera->view(), sizeof(glm::mat4));
		}
		else if (buttons[1]) {
			camera->zoom(dY * -0.005f);
			memcpy(mappedCameraView, &camera->view(), sizeof(glm::mat4));
		}
	}

	void scrollCallback(GLFWwindow*, double, double yoffset) {
		camera->zoom(static_cast<float>(yoffset) * 0.04f);
		memcpy(mappedCameraView, &camera->view(), sizeof(glm::mat4));
	}
}

struct Vertex
{
	glm::vec4 position;
	glm::vec4 color;
	glm::vec2 texCoord;

	/*
		The functions below allow us to access texture coordinates as input in the vertex shader. 
		That is necessary to be able to pass them to the fragment shader 
		for interpolation across the surface of the square
	*/

	static VkVertexInputBindingDescription getBindingDescription() 
	{
		VkVertexInputBindingDescription bindingDescription = {};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() 
	{
		std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, position);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

		return attributeDescriptions;
	}
};

struct CameraUBO
{
	glm::mat4 viewMatrix;
	glm::mat4 projectionMatrix;
};

struct ModelUBO
{
	glm::mat4 modelMatrix;
};

// Reference: https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Render_passes
VkRenderPass CreateRenderPass()
{	
    // Color buffer attachment represented by one of the images from the swap chain
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = swapchain->GetImageFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Create an attachment reference to be used with subpass
    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Create subpass description
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    // Specify subpass dependency
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // Create render pass
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkRenderPass renderPass;
    if (vkCreateRenderPass(device->GetVulkanDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
    }

    return renderPass;
}

/*
	Reference: https://vulkan-tutorial.com/Uniform_buffers 
	Look for UBO's declared above
*/
VkDescriptorSetLayout CreateDescriptorSetLayout(std::vector<VkDescriptorSetLayoutBinding> layoutBindings)
{
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = nullptr;
	descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
	descriptorSetLayoutCreateInfo.pBindings = layoutBindings.data();

	VkDescriptorSetLayout descriptorSetLayout;
	vkCreateDescriptorSetLayout(device->GetVulkanDevice(), &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout);
	return descriptorSetLayout;
}

// Reference: https://vulkan-tutorial.com/Uniform_buffers/Descriptor_pool_and_sets
VkDescriptorPool CreateDescriptorPool()
{
	// Info for the types of descriptors that can be allocated from this pool

	//compute and graphics descriptor sets are allocated from the same pool
	// Size 3 for camera, model, and storage descriptor sets
	VkDescriptorPoolSize poolSizes[4];    // Update if you add more descriptor sets 

	// Camera
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = 1;

	// Model
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[1].descriptorCount = 1;

	// Compute (modifies vertex buffer)
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[2].descriptorCount = 1;

	// Samplers
	poolSizes[3].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[3].descriptorCount = 1;

	VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
	descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolInfo.pNext = nullptr;
	descriptorPoolInfo.poolSizeCount = 4;				// Update if you add more descriptor sets 
	descriptorPoolInfo.pPoolSizes = poolSizes;
	descriptorPoolInfo.maxSets = 4;						// Update if you add more descriptor sets 

	VkDescriptorPool descriptorPool;
	vkCreateDescriptorPool(device->GetVulkanDevice(), &descriptorPoolInfo, nullptr, &descriptorPool);
	return descriptorPool;
}

VkDescriptorSet CreateDescriptorSet(VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout)
{
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &descriptorSetLayout;

	VkDescriptorSet descriptorSet;
	vkAllocateDescriptorSets(device->GetVulkanDevice(), &allocInfo, &descriptorSet);
	return descriptorSet;
}

// Reference: https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Fixed_functions
VkPipelineLayout CreatePipelineLayout(std::vector<VkDescriptorSetLayout> descriptorSetLayouts)
{
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
	pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = 0;

	VkPipelineLayout pipelineLayout;
	if (vkCreatePipelineLayout(device->GetVulkanDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) 
	{
		throw std::runtime_error("Failed to create pipeline layout");
	}

	return pipelineLayout;
}

// Reference: https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Conclusion
// Reference: https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Fixed_functions
// function argument 'subpass': index of the sub pass where this graphics pipeline will be used.
VkPipeline CreateGraphicsPipeline(VkPipelineLayout pipelineLayout, VkRenderPass renderPass, unsigned int subpass)
{
	//--------------------------------------------------------
	//---------------- Set up shader stages ------------------
	//--------------------------------------------------------
	// Can add more shader modules to the list of shader stages that are part of the overall graphics pipeline
	// Reference: https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Shader_modules
	// Create vert and frag shader modules
	VkShaderModule vertShaderModule = createShaderModule("CloudScapes/shaders/shader.vert.spv", device->GetVulkanDevice());
	VkShaderModule fragShaderModule = createShaderModule("CloudScapes/shaders/shader.frag.spv", device->GetVulkanDevice());
	
	// Assign each shader module to the appropriate stage in the pipeline
	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";

	//Add Shadermodules to the list of shader stages
	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	//--------------------------------------------------------
	//------------- Set up fixed-function stages -------------
	//--------------------------------------------------------

	// -------- Vertex input binding --------
	// Tell Vulkan how to pass this data format to the vertex shader once it's been uploaded to GPU memory 
	// Vertex binding describes at which rate to load data from memory throughout the vertices
	VkVertexInputBindingDescription vertexInputBinding = Vertex::getBindingDescription();
	// Inpute attribute bindings describe shader attribute locations and memory layouts
	std::array<VkVertexInputAttributeDescription, 3> vertexInputAttributes = Vertex::getAttributeDescriptions();

	// -------- Vertex input --------
	// Because we're hard coding the vertex data directly in the vertex shader, we'll fill in this structure to specify 
	// that there is no vertex data to load for now. We'll write the vertex buffers later.
	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &vertexInputBinding;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
	vertexInputInfo.pVertexAttributeDescriptions = vertexInputAttributes.data();

	// -------- Input assembly --------
	// The VkPipelineInputAssemblyStateCreateInfo struct describes two things: what kind of geometry will be drawn 
	// from the vertices and if primitive restart should be enabled.
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	// Viewports and Scissors (rectangles that define in which regions pixels are stored)
	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(swapchain->GetExtent().width);
	viewport.height = static_cast<float>(swapchain->GetExtent().height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	// While viewports define the transformation from the image to the framebuffer, 
	// scissor rectangles define in which regions pixels will actually be stored.
	// we simply want to draw to the entire framebuffer, so we'll specify a scissor rectangle that covers the framebuffer entirely
	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = swapchain->GetExtent();

	// Now this viewport and scissor rectangle need to be combined into a viewport state using the 
	// VkPipelineViewportStateCreateInfo struct. It is possible to use multiple viewports and scissor
	// rectangles on some graphics cards, so its members reference an array of them. Using multiple requires 
	// enabling a GPU feature (see logical device creation).
	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	// -------- Rasterize --------
	// The rasterizer takes the geometry that is shaped by the vertices from the vertex shader and turns
	// it into fragments to be colored by the fragment shader.
	// It also performs depth testing, face culling and the scissor test, and it can be configured to output 
	// fragments that fill entire polygons or just the edges (wireframe rendering). All this is configured 
	// using the VkPipelineRasterizationStateCreateInfo structure.
	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE; // If rasterizerDiscardEnable is set to VK_TRUE, then geometry never 
												   // passes through the rasterizer stage. This basically disables any output to the framebuffer.
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE; // The rasterizer can alter the depth values by adding a constant value or biasing 
										   // them based on a fragment's slope. This is sometimes used for shadow mapping, but 
										   // we won't be using it.Just set depthBiasEnable to VK_FALSE.
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	// -------- Multisampling --------
	// (turned off here)
	// The VkPipelineMultisampleStateCreateInfo struct configures multisampling, which is one of the ways to perform anti-aliasing. 
	// It works by combining the fragment shader results of multiple polygons that rasterize to the same pixel. This mainly occurs 
	// along edges, which is also where the most noticeable aliasing artifacts occur. Because it doesn't need to run the fragment 
	// shader multiple times if only one polygon maps to a pixel, it is significantly less expensive than simply rendering to a 
	// higher resolution and then downscaling. Enabling it requires enabling a GPU feature.
	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	// -------- Depth and Stencil Testing --------
	// If using Depth and Stencil Tests enable them here

	// -------- Color Blending ---------
	// (turned off here, but showing options for learning) --> Color Blending is usually for transparency
	// --> Configuration per attached framebuffer
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	// --> Global color blending settings
	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	// -------- Create graphics pipeline ---------
	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2; //vert and frag --> change if you add more
	pipelineInfo.pStages = shaderStages; //defined above
	pipelineInfo.pVertexInputState = &vertexInputInfo; //defined above
	pipelineInfo.pInputAssemblyState = &inputAssembly; //defined above
	pipelineInfo.pViewportState = &viewportState; //defined above
	pipelineInfo.pRasterizationState = &rasterizer; //defined above
	pipelineInfo.pMultisampleState = &multisampling; //defined above
	pipelineInfo.pDepthStencilState = nullptr; //defined above
	pipelineInfo.pColorBlendState = &colorBlending; //defined above
	pipelineInfo.pDynamicState = nullptr; //defined above
	pipelineInfo.layout = pipelineLayout; // passed in
	pipelineInfo.renderPass = renderPass; // passed in
	pipelineInfo.subpass = subpass; // passed in
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Vulkan allows you to create a new graphics pipeline by deriving from an existing pipeline. We aren't doing this.
	pipelineInfo.basePipelineIndex = -1;

	VkPipeline pipeline;
	if (vkCreateGraphicsPipelines(device->GetVulkanDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create pipeline");
	}

	// No need for the shader modules anymore, so we destory them!
	vkDestroyShaderModule(device->GetVulkanDevice(), vertShaderModule, nullptr);
	vkDestroyShaderModule(device->GetVulkanDevice(), fragShaderModule, nullptr);

	return pipeline;
}

VkPipeline CreateComputePipeline(VkPipelineLayout pipelineLayout)
{
	VkShaderModule compShaderModule = createShaderModule("CloudScapes/shaders/shader.comp.spv", device->GetVulkanDevice());

	VkPipelineShaderStageCreateInfo compShaderStageInfo = {};
	compShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	compShaderStageInfo.module = compShaderModule;
	compShaderStageInfo.pName = "main";

	VkComputePipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.stage = compShaderStageInfo;
	pipelineInfo.layout = pipelineLayout;

	VkPipeline pipeline;
	if (vkCreateComputePipelines(device->GetVulkanDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create pipeline");
	}

	vkDestroyShaderModule(device->GetVulkanDevice(), compShaderModule, nullptr);

	return pipeline;
}

std::vector<VkFramebuffer> CreateFrameBuffers(VkRenderPass renderPass) 
{
    std::vector<VkFramebuffer> frameBuffers(swapchain->GetCount());
    for (uint32_t i = 0; i < swapchain->GetCount(); i++) {
        VkImageView attachments[] = { swapchain->GetImageView(i) };

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchain->GetExtent().width;
        framebufferInfo.height = swapchain->GetExtent().height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device->GetVulkanDevice(), &framebufferInfo, nullptr, &frameBuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer");
        }
    }
    return frameBuffers;
}

int main(int argc, char** argv) 
{
    static constexpr char* applicationName = "CIS 565: Final Project -- Vertical Multiple Dusks";
	int window_height = 480;
	int window_width = 640;
    InitializeWindow(window_width, window_height, applicationName);

    unsigned int glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	// Vulkan Instance
    instance = new VulkanInstance(applicationName, glfwExtensionCount, glfwExtensions);

	// Drawing Surface, i.e. window where things are rendered to
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance->GetVkInstance(), GetGLFWWindow(), nullptr, &surface) != VK_SUCCESS) 
	{
        throw std::runtime_error("Failed to create window surface");
    }

	// physical Device --> GPU
	// TransferBit tells Vulkan that we can transfer data between CPU and GPU
    instance->PickPhysicalDevice({ VK_KHR_SWAPCHAIN_EXTENSION_NAME }, QueueFlagBit::GraphicsBit | QueueFlagBit::TransferBit | QueueFlagBit::ComputeBit | QueueFlagBit::PresentBit, surface);

	// Device --> Logical Device: Communicates with the Physical Device and generally acts as an interface to the Physical device
	// Reference: https://vulkan-tutorial.com/Drawing_a_triangle/Presentation
	// QueueFlagBit::PresentBit --> Vulkan is trying to determine if the Queue we  will setup to send commands through can 
	// actually 'present' images, i.e display them
	// QueueFlagBit::PresentBit --> Vulkan is trying to determine if we can make use of the window surface we just created, i.e. draw on the window
    device = instance->CreateDevice(QueueFlagBit::GraphicsBit | QueueFlagBit::TransferBit | QueueFlagBit::ComputeBit | QueueFlagBit::PresentBit);
    swapchain = device->CreateSwapChain(surface);

	// Command pools manage the memory that is used to store the command buffers, and command buffers are allocated from them.
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	// Command buffers are executed by submitting them on one of the device queues, like the graphics and presentation queues 
	// we retrieved. Each command pool can only allocate command buffers that are submitted on a single type of queue. We're 
	// going to record commands for drawing, which is why we've chosen the graphics queue family.
    poolInfo.queueFamilyIndex = instance->GetQueueFamilyIndices()[QueueFlags::Graphics];
    poolInfo.flags = 0;

    VkCommandPool commandPool;
    if (vkCreateCommandPool(device->GetVulkanDevice(), &poolInfo, nullptr, &commandPool) != VK_SUCCESS) 
	{
        throw std::runtime_error("Failed to create command pool");
    }



	// Create cloud textures
	VkImage textureImage;
	VkDeviceMemory textureImageMemory;
	loadTexture(device, commandPool, "../../src/CloudScapes/textures/meghanatheminion.jpg", &textureImage, &textureImageMemory, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VkImageView textureImageView;
	createTextureImageView(device, textureImageView, textureImage, VK_FORMAT_R8G8B8A8_UNORM);

	VkSampler textureSampler;
	createTextureSampler(device, textureSampler);




	// Create Camera and Model data to send over to shaders through descriptor sets 
	CameraUBO cameraTransforms;
	camera = new Camera(&cameraTransforms.viewMatrix);
	cameraTransforms.projectionMatrix = glm::perspective(static_cast<float>(45 * M_PI / 180), 640.f / 480.f, 0.1f, 1000.f);

	ModelUBO modelTransforms;
	modelTransforms.modelMatrix = glm::rotate(glm::mat4(1.f), static_cast<float>(15 * M_PI / 180), glm::vec3(0.f, 0.f, 1.f));

	// Create vertices and indices vectors to bind to buffers
	const std::vector<Vertex> vertices = {
		{ { -0.5f, -0.5f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },
		{ {  0.5f, -0.5f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } },
		{ {  0.5f,  0.5f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 0.0f, 1.0f } },
		{ { -0.5f,  0.5f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } }
	};

	std::vector<unsigned int> indices = { 0, 1, 2, 2, 3, 0};

	unsigned int vertexBufferSize = static_cast<uint32_t>(vertices.size() * sizeof(vertices[0]));
	unsigned int indexBufferSize = static_cast<uint32_t>(indices.size() * sizeof(indices[0]));

	// Create vertex and index buffers
	VkBuffer vertexBuffer = BufferUtils::CreateBuffer(device, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, vertexBufferSize);
	VkBuffer indexBuffer = BufferUtils::CreateBuffer(device, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBufferSize);
	unsigned int vertexBufferOffsets[2];

	// VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT bit indicates that memory allocated with this type can be mapped for host access using vkMapMemory
	// VK_MEMORY_PROPERTY_HOST_COHERENT_BIT allows to transfer host writes to the device or make device writes visible to the host (without needing other cache mgmt commands)
	VkDeviceMemory vertexBufferMemory = BufferUtils::AllocateMemoryForBuffers(device, { vertexBuffer, indexBuffer }, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexBufferOffsets);

	// Copy data to buffer memory
	{
		char* data;
		// The size is equal to vertexBufferOffsets + indexBufferSize 
		// vertexBufferOffsets stores the amount of memory and is calculated in the call to AllocateMemoryForBuffers above
		vkMapMemory(device->GetVulkanDevice(), vertexBufferMemory, 0, vertexBufferOffsets[1] + indexBufferSize, 0, reinterpret_cast<void**>(&data));
		memcpy(data + vertexBufferOffsets[0], vertices.data(), vertexBufferSize);
		memcpy(data + vertexBufferOffsets[1], indices.data(), indexBufferSize);
		vkUnmapMemory(device->GetVulkanDevice(), vertexBufferMemory);
	}

	// Bind the memory to the buffers
	BufferUtils::BindMemoryForBuffers(device, vertexBufferMemory, { vertexBuffer, indexBuffer }, vertexBufferOffsets);

	// Create uniform buffers 
	VkBuffer cameraBuffer = BufferUtils::CreateBuffer(device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(CameraUBO));
	VkBuffer modelBuffer = BufferUtils::CreateBuffer(device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(ModelUBO));
	unsigned int uniformBufferOffsets[2];
	VkDeviceMemory uniformBufferMemory = BufferUtils::AllocateMemoryForBuffers(device, { cameraBuffer, modelBuffer }, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBufferOffsets);

	// Copy data to uniform memory 
	{
		char* data;
		vkMapMemory(device->GetVulkanDevice(), uniformBufferMemory, 0, uniformBufferOffsets[1] + sizeof(ModelUBO), 0, reinterpret_cast<void**>(&data));		// reinterpret_cast = Leave data as bits, don't reformat it
		memcpy(data + uniformBufferOffsets[0], &cameraTransforms, sizeof(CameraUBO));
		memcpy(data + uniformBufferOffsets[1], &modelTransforms, sizeof(ModelUBO));
		vkUnmapMemory(device->GetVulkanDevice(), uniformBufferMemory);
	}

	// Bind memory to buffers
	BufferUtils::BindMemoryForBuffers(device, uniformBufferMemory, { cameraBuffer, modelBuffer }, uniformBufferOffsets);

	// Begin Descriptor logic
	VkDescriptorPool descriptorPool = CreateDescriptorPool();

	// Create the array of VkDescriptorSetLayoutBinding's 
	// Binding --> used in shader
	// Descriptor Type --> type of descriptor
	// Descriptor Count --> Shader variable can represent an array of UBO's, descriptorCount specifies number of values in the array
	// Stage Flags --> which shader you're referencing this descriptor from 
	// pImmutableSamplers --> for image sampling related descriptors

	VkDescriptorSetLayout computeSetLayout = CreateDescriptorSetLayout({
		{ 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
	});

	VkDescriptorSetLayout cameraSetLayout = CreateDescriptorSetLayout({
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
	});

	VkDescriptorSetLayout modelSetLayout = CreateDescriptorSetLayout({
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
	});
	
	VkDescriptorSetLayout samplerSetLayout = CreateDescriptorSetLayout({
		{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
	});

	// Initialize descriptor sets
	VkDescriptorSet computeSet = CreateDescriptorSet(descriptorPool, computeSetLayout);
	VkDescriptorSet cameraSet = CreateDescriptorSet(descriptorPool, cameraSetLayout);
	VkDescriptorSet modelSet = CreateDescriptorSet(descriptorPool, modelSetLayout);
	VkDescriptorSet samplerSet = CreateDescriptorSet(descriptorPool, samplerSetLayout);

	{
		// Compute
		VkDescriptorBufferInfo computeBufferInfo = {};
		computeBufferInfo.buffer = vertexBuffer;
		computeBufferInfo.offset = 0;
		computeBufferInfo.range = vertexBufferSize;

		VkWriteDescriptorSet writeComputeInfo = {};
		writeComputeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeComputeInfo.dstSet = computeSet;
		writeComputeInfo.dstBinding = 0;
		writeComputeInfo.descriptorCount = 1;									// How many 
		writeComputeInfo.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writeComputeInfo.pBufferInfo = &computeBufferInfo;

		// Camera 
		VkDescriptorBufferInfo cameraBufferInfo = {};
		cameraBufferInfo.buffer = cameraBuffer;
		cameraBufferInfo.offset = 0;
		cameraBufferInfo.range = sizeof(CameraUBO);

		VkWriteDescriptorSet writeCameraInfo = {};
		writeCameraInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeCameraInfo.dstSet = cameraSet;
		writeCameraInfo.dstBinding = 0;
		writeCameraInfo.descriptorCount = 1;									// How many 
		writeCameraInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeCameraInfo.pBufferInfo = &cameraBufferInfo;

		// Model
		VkDescriptorBufferInfo modelBufferInfo = {};
		modelBufferInfo.buffer = modelBuffer;
		modelBufferInfo.offset = 0;
		modelBufferInfo.range = sizeof(ModelUBO);

		VkWriteDescriptorSet writeModelInfo = {};
		writeModelInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeModelInfo.dstSet = modelSet;
		writeModelInfo.dstBinding = 0;
		writeModelInfo.descriptorCount = 1;
		writeModelInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeModelInfo.pBufferInfo = &modelBufferInfo;

		// Texture
		VkDescriptorImageInfo imageInfo = {};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = textureImageView;
		imageInfo.sampler = textureSampler;

		VkWriteDescriptorSet writeSamplerInfo = {};
		writeSamplerInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeSamplerInfo.dstSet = samplerSet;
		writeSamplerInfo.dstBinding = 0;
		writeSamplerInfo.dstArrayElement = 0;
		writeSamplerInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeSamplerInfo.descriptorCount = 1;
		writeSamplerInfo.pImageInfo = &imageInfo;

		VkWriteDescriptorSet writeDescriptorSets[] = { writeComputeInfo, writeCameraInfo, writeModelInfo, writeSamplerInfo };

		vkUpdateDescriptorSets(device->GetVulkanDevice(), 4, writeDescriptorSets, 0, nullptr);
	}


    VkRenderPass renderPass = CreateRenderPass();
	VkPipelineLayout computePipelineLayout = CreatePipelineLayout({ computeSetLayout });
	VkPipeline computePipeline = CreateComputePipeline(computePipelineLayout);

	VkPipelineLayout graphicsPipelineLayout = CreatePipelineLayout({ cameraSetLayout, modelSetLayout, samplerSetLayout });
	VkPipeline graphicsPipeline = CreateGraphicsPipeline(graphicsPipelineLayout, renderPass, 0);

    // Create one framebuffer for each frame of the swap chain
    std::vector<VkFramebuffer> frameBuffers = CreateFrameBuffers(renderPass);

    // Create one command buffer for each frame of the swap chain
    std::vector<VkCommandBuffer> commandBuffers(swapchain->GetCount());

    // Specify the command pool and number of buffers to allocate
	/*
	The level parameter specifies if the allocated command buffers are primary or secondary command buffers.
		VK_COMMAND_BUFFER_LEVEL_PRIMARY: Can be submitted to a queue for execution, but cannot be called from other command buffers.
		VK_COMMAND_BUFFER_LEVEL_SECONDARY : Cannot be submitted directly, but can be called from primary command buffers.
	*/
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = (uint32_t)(commandBuffers.size());

    if (vkAllocateCommandBuffers(device->GetVulkanDevice(), &commandBufferAllocateInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers");
    }

    // Record command buffers, one for each frame of the swapchain
    for (unsigned int i = 0; i < commandBuffers.size(); ++i) {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT; // How we are using the command buffer
																		// The command buffer can be resubmitted while it is also already pending execution.
        beginInfo.pInheritanceInfo = nullptr; //only useful for secondary command buffers

		//---------- Begin recording ----------
		//If the command buffer was already recorded once, then a call to vkBeginCommandBuffer will implicitly reset it. 
		// It's not possible to append commands to a buffer at a later time.
        vkBeginCommandBuffer(commandBuffers[i], &beginInfo);

        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = frameBuffers[i]; //attachments we bind to the renderpass
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = swapchain->GetExtent();

		// Clear values used while clearing the attachments before usage or after usage
        VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

		//-----------------------------------------------------
		//--- Compute Pipeline Binding, Dispatch & Barriers ---
		//-----------------------------------------------------

		//Bind the compute piepline
		vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);

		//Bind Descriptor Sets for compute
		vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeSet, 0, nullptr);

		// Dispatch the compute kernel, with one thread for each vertex
		vkCmdDispatch(commandBuffers[i], vertices.size(), 1, 1); //similar to kernel call --> void vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);

		// Define a memory barrier to transition the vertex buffer from a compute storage object to a vertex input
		// Each element of the pMemoryBarriers, pBufferMemoryBarriers and pImageMemoryBarriers arrays specifies two halves of a memory dependency, as defined above.
		// Reference: https://vulkan.lunarg.com/doc/view/1.0.30.0/linux/vkspec.chunked/ch06s05.html#synchronization-memory-barriers
		VkBufferMemoryBarrier computeToVertexBarrier = {};
		computeToVertexBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		computeToVertexBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
		computeToVertexBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
		computeToVertexBarrier.srcQueueFamilyIndex = device->GetQueueIndex(QueueFlags::Compute);
		computeToVertexBarrier.dstQueueFamilyIndex = device->GetQueueIndex(QueueFlags::Graphics);
		computeToVertexBarrier.buffer = vertexBuffer;
		computeToVertexBarrier.offset = 0;
		computeToVertexBarrier.size = vertexBufferSize;

		// A pipeline barrier inserts an execution dependency and a set of memory dependencies between a set of commands earlier in the command buffer and a set of commands later in the command buffer.
		// Reference: https://vulkan.lunarg.com/doc/view/1.0.30.0/linux/vkspec.chunked/ch06s05.html
		vkCmdPipelineBarrier(commandBuffers[i],
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
			0,
			0, nullptr,
			1, &computeToVertexBarrier,
			0, nullptr);

		//----------------------------------------------
		//--- Graphics Pipeline Binding and Dispatch ---
		//----------------------------------------------
        vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		// VK_SUBPASS_CONTENTS_INLINE: The render pass commands will be embedded in the primary command
		// buffer itself and no secondary command buffers will be executed.

		// Bind camera descriptor set
		vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout, 0, 1, &cameraSet, 0, nullptr);

		// Bind model descriptor set
		vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout, 1, 1, &modelSet, 0, nullptr);
				
		// Bind sampler descriptor set
		vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout, 2, 1, &samplerSet, 0, nullptr);

		// Bind the graphics pipeline
		vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		// Bind the vertex and index buffers
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, &vertexBuffer, offsets);

		// Bind triangle index buffer
		vkCmdBindIndexBuffer(commandBuffers[i], indexBuffer, 0, VK_INDEX_TYPE_UINT32);

		// Draw
		/*
			vkCmdDraw has the following parameters, aside from the command buffer:
				vertexCount: Even though we don't have a vertex buffer, we technically still have 3 vertices to draw.
				instanceCount: Used for instanced rendering, use 1 if you're not doing that.
				firstVertex: Used as an offset into the vertex buffer, defines the lowest value of gl_VertexIndex.
				firstInstance: Used as an offset for instanced rendering, defines the lowest value of gl_InstanceIndex.
		*/
		//vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);

		// Draw indexed triangle
		vkCmdDrawIndexed(commandBuffers[i], static_cast<uint32_t>(indices.size()), 1, 0, 0, 1);

        vkCmdEndRenderPass(commandBuffers[i]);

		//---------- End Recording ----------
        if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to record command buffer");
        }
    }// end for loop

	glfwSetMouseButtonCallback(GetGLFWWindow(), mouseButtonCallback);
	glfwSetCursorPosCallback(GetGLFWWindow(), cursorPosCallback);
	glfwSetScrollCallback(GetGLFWWindow(), scrollCallback);

	int frameNumber = 0;
	// Map the part of the buffer referring the camera view matrix so it can be updated when the camera moves
	vkMapMemory(device->GetVulkanDevice(), uniformBufferMemory, uniformBufferOffsets[0] + offsetof(CameraUBO, viewMatrix), sizeof(glm::mat4), 0, reinterpret_cast<void**>(&mappedCameraView));


	// Reference: https://vulkan-tutorial.com/Drawing_a_triangle/Drawing/Rendering_and_presentation
    while (!ShouldQuit()) 
	{
        swapchain->Acquire();
        
        // Submit the command buffer
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		
		/*
			Can synchronize between queues by using certain supported features
				VkEvent: Versatile waiting, but limited to a single queue
				VkSemaphore: GPU to GPU synchronization
				vkFence: GPU to CPU synchronization
		*/

        VkSemaphore waitSemaphores[] = { swapchain->GetImageAvailableSemaphore() };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		// These parameters specify which semaphores to wait on before execution begins and in which stage(s) of the pipeline to wait
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
		// We want to wait with writing colors to the image until it's available, so we're specifying the stage of the graphics pipeline 
		// that writes to the color attachment. That means that theoretically the implementation can already start executing our vertex 
		// shader and such while the image is not available yet.
		
		// specify which command buffers to actually submit for execution
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[swapchain->GetIndex()];
		
		// The signalSemaphoreCount and pSignalSemaphores parameters specify which semaphores to signal once the command buffer(s) have finished execution.
        VkSemaphore signalSemaphores[] = { swapchain->GetRenderFinishedSemaphore() };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
		
		// submit the command buffer to the graphics queue
        if (vkQueueSubmit(device->GetQueue(QueueFlags::Graphics), 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit draw command buffer");
        }
    
        swapchain->Present();

        glfwPollEvents();
    }// end while loop


	vkUnmapMemory(device->GetVulkanDevice(), uniformBufferMemory);


    // Wait for the device to finish executing before cleanup
    vkDeviceWaitIdle(device->GetVulkanDevice());

	//---------------------
	//------ CleanUp ------
	//---------------------

	delete camera;

	//TODO: Delete texture image texture image memory
	vkDestroyImage(device->GetVulkanDevice(), textureImage, nullptr);
	vkFreeMemory(device->GetVulkanDevice(), textureImageMemory, nullptr);
	vkDestroyImageView(device->GetVulkanDevice(), textureImageView, nullptr);
	vkDestroySampler(device->GetVulkanDevice(), textureSampler, nullptr);

	vkDestroyBuffer(device->GetVulkanDevice(), vertexBuffer, nullptr);
	vkDestroyBuffer(device->GetVulkanDevice(), indexBuffer, nullptr);
	vkFreeMemory(device->GetVulkanDevice(), vertexBufferMemory, nullptr);

	vkDestroyBuffer(device->GetVulkanDevice(), cameraBuffer, nullptr);
	vkDestroyBuffer(device->GetVulkanDevice(), modelBuffer, nullptr);
	vkFreeMemory(device->GetVulkanDevice(), uniformBufferMemory, nullptr);

	vkDestroyDescriptorSetLayout(device->GetVulkanDevice(), computeSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(device->GetVulkanDevice(), cameraSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(device->GetVulkanDevice(), modelSetLayout, nullptr);
	vkDestroyDescriptorPool(device->GetVulkanDevice(), descriptorPool, nullptr);

	vkDestroyPipeline(device->GetVulkanDevice(), computePipeline, nullptr);
	vkDestroyPipelineLayout(device->GetVulkanDevice(), computePipelineLayout, nullptr);

	vkDestroyPipeline(device->GetVulkanDevice(), graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(device->GetVulkanDevice(), graphicsPipelineLayout, nullptr);

    vkDestroyRenderPass(device->GetVulkanDevice(), renderPass, nullptr);
    vkFreeCommandBuffers(device->GetVulkanDevice(), commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());

    for (size_t i = 0; i < frameBuffers.size(); i++) 
	{
        vkDestroyFramebuffer(device->GetVulkanDevice(), frameBuffers[i], nullptr);
    }

    delete swapchain;
    vkDestroyCommandPool(device->GetVulkanDevice(), commandPool, nullptr);
    vkDestroySurfaceKHR(instance->GetVkInstance(), surface, nullptr);
    delete device;
    delete instance;
    DestroyWindow();
}