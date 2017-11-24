#include "Renderer.h"

struct Vertex
{
	glm::vec4 position;
	glm::vec3 color;
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
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

		return attributeDescriptions;
	}
};

struct ModelUBO
{
	glm::mat4 modelMatrix;
};

std::vector<Vertex> vertices;

Renderer::Renderer(VulkanDevice* device, VulkanSwapChain* swapChain, Scene* scene, Camera* camera)
	: device(device), 
	logicalDevice(device->GetVkDevice()),
	swapChain(swapChain),
	scene(scene),
	camera(camera)
{
	InitializeRenderer();
}

Renderer::~Renderer() 
{
	vkDeviceWaitIdle(logicalDevice);

	// TODO: Destroy any resources you created
	vkFreeCommandBuffers(logicalDevice, graphicsCommandPool, static_cast<uint32_t>(graphicsCommandBuffer.size()), graphicsCommandBuffer.data());
	vkFreeCommandBuffers(logicalDevice, computeCommandPool, static_cast<uint32_t>(computeCommandBuffer.size()), computeCommandBuffer.data());

	vkDestroyCommandPool(logicalDevice, computeCommandPool, nullptr);
	vkDestroyCommandPool(logicalDevice, graphicsCommandPool, nullptr);

	DestroyFrameResources();

	vkDestroyPipelineLayout(logicalDevice, graphicsPipelineLayout, nullptr);
	vkDestroyPipelineLayout(logicalDevice, computePipelineLayout, nullptr);
	vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);
	vkDestroyPipeline(logicalDevice, computePipeline, nullptr);
	vkDestroyRenderPass(logicalDevice, renderPass, nullptr);

	vkDestroyDescriptorSetLayout(logicalDevice, computeSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, cameraSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, modelSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, samplerSetLayout, nullptr);
	vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);

	vkDestroySampler(logicalDevice, textureSampler, nullptr);
	vkDestroyImageView(logicalDevice, textureImageView, nullptr);

	vkDestroyImage(logicalDevice, textureImage, nullptr);
	vkFreeMemory(logicalDevice, textureImageMemory, nullptr);

	vkDestroyBuffer(logicalDevice, vertexBuffer, nullptr);
	vkDestroyBuffer(logicalDevice, indexBuffer, nullptr);
	vkFreeMemory(logicalDevice, vertexBufferMemory, nullptr);
	vkDestroyBuffer(logicalDevice, modelBuffer, nullptr);
	vkFreeMemory(logicalDevice, modelBufferMemory, nullptr);
}

void Renderer::InitializeRenderer()
{
	CreateCommandPools();
	CreateRenderPass();

	CreateDescriptorPool();
	CreateAllDescriptorSetLayouts();
	CreateAllDescriptorSets();

	CreateFrameResources();

	computePipelineLayout = CreatePipelineLayout({ computeSetLayout });
	CreateComputePipeline();

	graphicsPipelineLayout = CreatePipelineLayout({ cameraSetLayout, modelSetLayout, samplerSetLayout });
	CreateGraphicsPipeline(renderPass, 0);

	RecordGraphicsCommandBuffer();
	RecordComputeCommandBuffer();
}

//This Function submits command buffers for execution --> so that the application can 
//actually present one image after another and not just stop after the first image
void Renderer::Frame()
{
	//-------------------------------------------
	//--------- Submit Compute Queue ------------
	//-------------------------------------------

	VkSubmitInfo computeSubmitInfo = {};
	computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	computeSubmitInfo.commandBufferCount = 1;
	computeSubmitInfo.pCommandBuffers = &computeCommandBuffer[swapChain->GetIndex()];

	// submit the command buffer to the compute queue
	if (vkQueueSubmit(device->GetQueue(QueueFlags::Compute), 1, &computeSubmitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
		throw std::runtime_error("Failed to submit compute command buffer");
	}

	//-------------------------------------------
	//--------- Submit Graphics Queue -----------
	//-------------------------------------------
	swapChain->Acquire();

	// Submit the command buffer
	VkSubmitInfo graphicsSubmitInfo = {};
	graphicsSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	/*
	Can synchronize between queues by using certain supported features
	VkEvent: Versatile waiting, but limited to a single queue
	VkSemaphore: GPU to GPU synchronization
	vkFence: GPU to CPU synchronization
	*/

	VkSemaphore waitSemaphores[] = { swapChain->GetImageAvailableVkSemaphore() };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	// These parameters specify which semaphores to wait on before execution begins and in which stage(s) of the pipeline to wait
	graphicsSubmitInfo.waitSemaphoreCount = 1;
	graphicsSubmitInfo.pWaitSemaphores = waitSemaphores;
	graphicsSubmitInfo.pWaitDstStageMask = waitStages;
	// We want to wait with writing colors to the image until it's available, so we're specifying the stage of the graphics pipeline 
	// that writes to the color attachment. That means that theoretically the implementation can already start executing our vertex 
	// shader and such while the image is not available yet.

	// specify which command buffers to actually submit for execution
	graphicsSubmitInfo.commandBufferCount = 1;
	graphicsSubmitInfo.pCommandBuffers = &graphicsCommandBuffer[swapChain->GetIndex()];

	// The signalSemaphoreCount and pSignalSemaphores parameters specify which semaphores to signal once the command buffer(s) have finished execution.
	VkSemaphore signalSemaphores[] = { swapChain->GetRenderFinishedVkSemaphore() };
	graphicsSubmitInfo.signalSemaphoreCount = 1;
	graphicsSubmitInfo.pSignalSemaphores = signalSemaphores;

	// submit the command buffer to the graphics queue
	if (vkQueueSubmit(device->GetQueue(QueueFlags::Graphics), 1, &graphicsSubmitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
		throw std::runtime_error("Failed to submit draw command buffer");
	}

	// Display a frame
	swapChain->Present();
}

//----------------------------------------------
//-------------- Render Pass--------------------
//----------------------------------------------
// Reference: https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Render_passes
void Renderer::CreateRenderPass()
{
	// Color buffer attachment represented by one of the images from the swap chain
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = swapChain->GetVkImageFormat();
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// Create a reference for the color attachment to be used with subpass
	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Depth buffer attachment represented by one of the images from the swap chain
	VkAttachmentDescription depthAttachment = {};
	depthAttachment.format = findDepthFormat(); //The format should be the same as the depth image itself.
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; //This time we don't care about storing the depth data, 
																//because it will not be used after drawing has finished.
																//This may allow the hardware to perform additional optimizations.
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; //we don't care about the previous depth contents, 
															   //so we can use VK_IMAGE_LAYOUT_UNDEFINED as initialLayout
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// Create a reference for the depth attachment to be used with subpass
	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// Create subpass description
	// a subpass can only use a single depth (+stencil) attachment. 
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	// Specify subpass dependency
	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	// Create render pass
	std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment }; //Add more attachments here
	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());;
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(device->GetVkDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create render pass");
	}
}

//----------------------------------------------
//-------------- Pipelines ---------------------
//----------------------------------------------
// Reference: https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Fixed_functions
VkPipelineLayout Renderer::CreatePipelineLayout(std::vector<VkDescriptorSetLayout> descriptorSetLayouts)
{
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
	pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = 0;

	VkPipelineLayout pipelineLayout;
	if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create pipeline layout");
	}

	return pipelineLayout;
}

// Reference: https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Conclusion
// Reference: https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Fixed_functions
// function argument 'subpass': index of the sub pass where this graphics pipeline will be used.
void Renderer::CreateGraphicsPipeline(VkRenderPass renderPass, unsigned int subpass)
{
	//--------------------------------------------------------
	//---------------- Set up shader stages ------------------
	//--------------------------------------------------------
	// Can add more shader modules to the list of shader stages that are part of the overall graphics pipeline
	// Reference: https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Shader_modules
	// Create vert and frag shader modules
	VkShaderModule vertShaderModule = ShaderModule::createShaderModule("CloudScapes/shaders/shader.vert.spv", device->GetVkDevice());
	VkShaderModule fragShaderModule = ShaderModule::createShaderModule("CloudScapes/shaders/shader.frag.spv", device->GetVkDevice());

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
	viewport.width = static_cast<float>(swapChain->GetVkExtent().width);
	viewport.height = static_cast<float>(swapChain->GetVkExtent().height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	// While viewports define the transformation from the image to the framebuffer, 
	// scissor rectangles define in which regions pixels will actually be stored.
	// we simply want to draw to the entire framebuffer, so we'll specify a scissor rectangle that covers the framebuffer entirely
	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = swapChain->GetVkExtent();

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
	VkPipelineDepthStencilStateCreateInfo depthStencil = {};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

	//The depthTestEnable field specifies if the depth of new fragments should be compared to the 
	//depth buffer to see if they should be discarded. The depthWriteEnable field specifies if the 
	//new depth of fragments that pass the depth test should actually be written to the depth buffer. 
	//This is useful for drawing transparent objects. They should be compared to the previously rendered 
	//opaque objects, but not cause further away transparent objects to not be drawn.
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;

	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS; // specifies the comparison that is performed to keep or discard fragments.

	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f;
	depthStencil.maxDepthBounds = 1.0f;

	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.front = {};
	depthStencil.back = {};

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
	pipelineInfo.pDepthStencilState = &depthStencil; //defined above
	pipelineInfo.pColorBlendState = &colorBlending; //defined above
	pipelineInfo.pDynamicState = nullptr; //defined above
	pipelineInfo.layout = graphicsPipelineLayout; // passed in
	pipelineInfo.renderPass = renderPass; // passed in
	pipelineInfo.subpass = subpass; // passed in
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Vulkan allows you to create a new graphics pipeline by deriving from an existing pipeline. We aren't doing this.
	pipelineInfo.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(device->GetVkDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create pipeline");
	}

	// No need for the shader modules anymore, so we destory them!
	vkDestroyShaderModule(device->GetVkDevice(), vertShaderModule, nullptr);
	vkDestroyShaderModule(device->GetVkDevice(), fragShaderModule, nullptr);
}

void Renderer::CreateComputePipeline()
{
	VkShaderModule compShaderModule = ShaderModule::createShaderModule("CloudScapes/shaders/shader.comp.spv", device->GetVkDevice());

	VkPipelineShaderStageCreateInfo compShaderStageInfo = {};
	compShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	compShaderStageInfo.module = compShaderModule;
	compShaderStageInfo.pName = "main";

	VkComputePipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.stage = compShaderStageInfo;
	pipelineInfo.layout = computePipelineLayout;

	if (vkCreateComputePipelines(device->GetVkDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create pipeline");
	}

	vkDestroyShaderModule(device->GetVkDevice(), compShaderModule, nullptr);
}

//----------------------------------------------
//-------------- Frame Resources ---------------
//----------------------------------------------
void Renderer::CreateFrameResources()
{
	CreateImageViewsforFrame();
	createDepthResources();
	CreateFrameBuffers(renderPass);
}

void Renderer::DestroyFrameResources()
{
	// Destroy Image Views attached to frameBuffers
	for (size_t i = 0; i < imageViews.size(); i++) 
	{
		vkDestroyImageView(logicalDevice, imageViews[i], nullptr);
	}

	// Destroy Depth Image and ImageView
	vkDestroyImageView(logicalDevice, depthImageView, nullptr);
	vkFreeMemory(logicalDevice, depthImageMemory, nullptr);
	vkDestroyImage(logicalDevice, depthImage, nullptr);

	// Destroy FrameBuffers
	for (size_t i = 0; i < frameBuffers.size(); i++) 
	{
		vkDestroyFramebuffer(logicalDevice, frameBuffers[i], nullptr);
	}
}

void Renderer::RecreateFrameResources()
{
	//This Function recreates the frame resources which in turn means we need to recreate the graphics pipeline and rerecord the graphics command buffers
	vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(logicalDevice, graphicsPipelineLayout, nullptr);

	vkFreeCommandBuffers(logicalDevice, graphicsCommandPool, static_cast<uint32_t>(graphicsCommandBuffer.size()), graphicsCommandBuffer.data());

	DestroyFrameResources();
	CreateFrameResources();

	graphicsPipelineLayout = CreatePipelineLayout({ cameraSetLayout, modelSetLayout, samplerSetLayout });
	CreateGraphicsPipeline(renderPass, 0);

	RecordGraphicsCommandBuffer();
}

// Helper Functions for Frame Resources
void Renderer::CreateImageViewsforFrame()
{
	/*
	An image view is quite literally a view into an image.It describes how to access the image and which
	part of the image to access, for example if it should be treated as a 2D texture depth texture without
	any mipmapping levels.
	*/
	imageViews.resize(swapChain->GetCount());
	for (uint32_t i = 0; i < swapChain->GetCount(); i++) 
	{
		// --- Create an image view for each swap chain image ---
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = swapChain->GetVkImage(i);

		// Specify how the image data should be interpreted
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = swapChain->GetVkImageFormat();

		// Specify color channel mappings (can be used for swizzling)
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		// Describe the image's purpose and which part of the image should be accessed
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		// Create the image view
		if (vkCreateImageView(logicalDevice, &createInfo, nullptr, &imageViews[i]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create image views");
		}
	}
}

void Renderer::createDepthResources()
{
	VkFormat depthFormat = findDepthFormat();
	// Create Depth Image and ImageViews
	Image::createImage(device, swapChain->GetVkExtent().width, swapChain->GetVkExtent().height, depthFormat,
					   VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
					   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
	// Create Depth ImageView
	Image::createImageView(device, depthImageView, depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

	// Transition the image for use as depth-stencil
	Image::transitionImageLayout(device, graphicsCommandPool, depthImage, depthFormat,
								VK_IMAGE_LAYOUT_UNDEFINED,
								VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

void Renderer::CreateFrameBuffers(VkRenderPass renderPass)
{
	frameBuffers.resize(swapChain->GetCount());
	for (uint32_t i = 0; i < swapChain->GetCount(); i++)
	{
		std::array<VkImageView, 2> attachments = { swapChain->GetVkImageView(i), 
												   depthImageView };

		// The color attachment differs for every swap chain image, but the same depth image can 
		// be used by all of them because only a single subpass is running at the same time due to our semaphores.

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = swapChain->GetVkExtent().width;
		framebufferInfo.height = swapChain->GetVkExtent().height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &frameBuffers[i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create framebuffer");
		}
	}
}
//----------------------------------------------
//-------------- Command Pools -----------------
//----------------------------------------------
void Renderer::CreateCommandPools()
{
	VkCommandPoolCreateInfo graphicsPoolInfo = {};
	graphicsPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	graphicsPoolInfo.queueFamilyIndex = device->GetInstance()->GetQueueFamilyIndices()[QueueFlags::Graphics];
	graphicsPoolInfo.flags = 0;

	if (vkCreateCommandPool(logicalDevice, &graphicsPoolInfo, nullptr, &graphicsCommandPool) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create graphics command pool");
	}

	VkCommandPoolCreateInfo computePoolInfo = {};
	computePoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	computePoolInfo.queueFamilyIndex = device->GetInstance()->GetQueueFamilyIndices()[QueueFlags::Compute];
	computePoolInfo.flags = 0;

	if (vkCreateCommandPool(logicalDevice, &computePoolInfo, nullptr, &computeCommandPool) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create compute command pool");
	}
}

//----------------------------------------------
//-------------- Command Buffers ---------------
//----------------------------------------------
void Renderer::RecordGraphicsCommandBuffer()
{
	graphicsCommandBuffer.resize(swapChain->GetCount());

	// Specify the command pool and number of buffers to allocate
	/*
	The level parameter specifies if the allocated command buffers are primary or secondary command buffers.
	VK_COMMAND_BUFFER_LEVEL_PRIMARY: Can be submitted to a queue for execution, but cannot be called from other command buffers.
	VK_COMMAND_BUFFER_LEVEL_SECONDARY : Cannot be submitted directly, but can be called from primary command buffers.
	*/
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.commandPool = graphicsCommandPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = (uint32_t)(graphicsCommandBuffer.size());

	if (vkAllocateCommandBuffers(logicalDevice, &commandBufferAllocateInfo, graphicsCommandBuffer.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate graphics command buffers");
	}

	// Record graphics command buffers, one for each frame of the swapchain
	for (unsigned int i = 0; i < graphicsCommandBuffer.size(); ++i)
	{
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT; // How we are using the command buffer
																		// The command buffer can be resubmitted while it is also already pending execution.
		beginInfo.pInheritanceInfo = nullptr; //only useful for secondary command buffers

		//---------- Begin recording ----------
		//If the command buffer was already recorded once, then a call to vkBeginCommandBuffer will implicitly reset it. 
		// It's not possible to append commands to a buffer at a later time.
		vkBeginCommandBuffer(graphicsCommandBuffer[i], &beginInfo);

		VkRenderPassBeginInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = frameBuffers[i]; //attachments we bind to the renderpass
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = swapChain->GetVkExtent();

		// Clear values used while clearing the attachments before usage or after usage
		std::array<VkClearValue, 2> clearValues = {};
		clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f }; //black
		clearValues[1].depthStencil = { 1.0f, 0 }; //far plane

		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		//----------------------------------------------
		//--- Graphics Pipeline Binding and Dispatch ---
		//----------------------------------------------
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
		vkCmdPipelineBarrier(graphicsCommandBuffer[i],
							 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
							 VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
							 0,
							 0, nullptr,
							 1, &computeToVertexBarrier,
							 0, nullptr);

		vkCmdBeginRenderPass(graphicsCommandBuffer[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		// VK_SUBPASS_CONTENTS_INLINE: The render pass commands will be embedded in the primary command
		// buffer itself and no secondary command buffers will be executed.

		// Bind the graphics pipeline
		vkCmdBindPipeline(graphicsCommandBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		// Bind camera descriptor set
		vkCmdBindDescriptorSets(graphicsCommandBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout, 0, 1, &cameraSet, 0, nullptr);
		// Bind model descriptor set
		vkCmdBindDescriptorSets(graphicsCommandBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout, 1, 1, &modelSet, 0, nullptr);
		// Bind sampler descriptor set
		vkCmdBindDescriptorSets(graphicsCommandBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout, 2, 1, &samplerSet, 0, nullptr);

		// Bind the vertex and index buffers
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(graphicsCommandBuffer[i], 0, 1, &vertexBuffer, offsets);

		// Bind triangle index buffer
		vkCmdBindIndexBuffer(graphicsCommandBuffer[i], indexBuffer, 0, VK_INDEX_TYPE_UINT32);

		// Draw indexed triangle
		/*
		vkCmdDrawIndexed has the following parameters, aside from the command buffer:
		indexCount;
		instanceCount: Used for instanced rendering, use 1 if you're not doing that.
		firstIndex:  Used as an offset into the index buffer
		vertexOffset: Used as an offset into the vertex buffer
		firstInstance: Used as an offset for instanced rendering, defines the lowest value of gl_InstanceIndex.
		*/
		vkCmdDrawIndexed(graphicsCommandBuffer[i], indexBufferSize, 1, 0, 0, 1);

		vkCmdEndRenderPass(graphicsCommandBuffer[i]);

		//---------- End Recording ----------
		if (vkEndCommandBuffer(graphicsCommandBuffer[i]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to record the graphics command buffer");
		}
	}// end for loop
}

void Renderer::RecordComputeCommandBuffer()
{
	computeCommandBuffer.resize(swapChain->GetCount());

	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.commandPool = computeCommandPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = (uint32_t)(computeCommandBuffer.size());

	if (vkAllocateCommandBuffers(logicalDevice, &commandBufferAllocateInfo, computeCommandBuffer.data()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate compute command buffers");
	}

	// Record command buffers, one for each frame of the swapchain
	for (unsigned int i = 0; i < computeCommandBuffer.size(); ++i)
	{
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT; // How we are using the command buffer
																		// The command buffer can be resubmitted while it is also already pending execution.
		beginInfo.pInheritanceInfo = nullptr; //only useful for secondary command buffers

		//---------- Begin recording ----------
		//If the command buffer was already recorded once, then a call to vkBeginCommandBuffer will implicitly reset it. 
		// It's not possible to append commands to a buffer at a later time.
		vkBeginCommandBuffer(computeCommandBuffer[i], &beginInfo);

		//-----------------------------------------------------
		//--- Compute Pipeline Binding, Dispatch & Barriers ---
		//-----------------------------------------------------
		//Bind the compute piepline
		vkCmdBindPipeline(computeCommandBuffer[i], VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);

		//Bind Descriptor Sets for compute
		vkCmdBindDescriptorSets(computeCommandBuffer[i], VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeSet, 0, nullptr);

		// Dispatch the compute kernel, with one thread for each vertex
		// similar to a kernel call --> void vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);
		vkCmdDispatch(computeCommandBuffer[i], vertices.size(), 1, 1);

		//---------- End Recording ----------
		if (vkEndCommandBuffer(computeCommandBuffer[i]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to record the compute command buffer");
		}
	}// end for loop
}

//----------------------------------------------
//-------------- Descriptors -------------------
//----------------------------------------------
// Reference: https://vulkan-tutorial.com/Uniform_buffers/Descriptor_pool_and_sets
void Renderer::CreateDescriptorPool()
{
	// Info for the types of descriptors that can be allocated from this pool

	// compute and graphics descriptor sets are allocated from the same pool
	std::vector<VkDescriptorPoolSize> poolSizes = {
		// Format for elements: { type, descriptorCount }
		// Camera
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
		// Model
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
		// Compute (modifies vertex buffer)
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
		// Sampler
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }
	};

	VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
	descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolInfo.pNext = nullptr;
	descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size()); 
	descriptorPoolInfo.pPoolSizes = poolSizes.data();
	descriptorPoolInfo.maxSets = static_cast<uint32_t>(poolSizes.size()); 

	if (vkCreateDescriptorPool(device->GetVkDevice(), &descriptorPoolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create descriptor pool");
	}
}

// Reference: https://vulkan-tutorial.com/Uniform_buffers
VkDescriptorSetLayout Renderer::CreateDescriptorSetLayout(std::vector<VkDescriptorSetLayoutBinding> layoutBindings)
{
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = nullptr;
	descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
	descriptorSetLayoutCreateInfo.pBindings = layoutBindings.data();

	VkDescriptorSetLayout descriptorSetLayout;
	vkCreateDescriptorSetLayout(device->GetVkDevice(), &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout);
	return descriptorSetLayout;
}

VkDescriptorSet Renderer::CreateDescriptorSet(VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout)
{
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &descriptorSetLayout;

	VkDescriptorSet descriptorSet;
	vkAllocateDescriptorSets(device->GetVkDevice(), &allocInfo, &descriptorSet);
	return descriptorSet;
}

void Renderer::CreateAllDescriptorSetLayouts()
{
	// Create the array of VkDescriptorSetLayoutBinding's 
	// Binding --> used in shader
	// Descriptor Type --> type of descriptor
	// Descriptor Count --> Shader variable can represent an array of UBO's, descriptorCount specifies number of values in the array
	// Stage Flags --> which shader you're referencing this descriptor from 
	// pImmutableSamplers --> for image sampling related descriptors

	computeSetLayout = CreateDescriptorSetLayout({
		{ 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
	});

	cameraSetLayout = CreateDescriptorSetLayout({
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
	});

	modelSetLayout = CreateDescriptorSetLayout({
		{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
	});

	samplerSetLayout = CreateDescriptorSetLayout({
		{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
	});
}

void Renderer::CreateAllDescriptorSets()
{
	// Initialize descriptor sets
	computeSet = CreateDescriptorSet(descriptorPool, computeSetLayout);
	cameraSet = CreateDescriptorSet(descriptorPool, cameraSetLayout);
	modelSet = CreateDescriptorSet(descriptorPool, modelSetLayout);
	samplerSet = CreateDescriptorSet(descriptorPool, samplerSetLayout);

	// Create Buffers that will be bound to descriptor Sets

	// Create Camera and Model data to send over to shaders through descriptor sets
	ModelUBO modelTransforms;
	modelTransforms.modelMatrix = glm::mat4(1.0f);

	// Create model buffer
	modelBuffer = BufferUtils::CreateBuffer(device, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(ModelUBO));
	unsigned int modelBufferOffset[1];
	modelBufferMemory = BufferUtils::AllocateMemoryForBuffers(device,
															{ modelBuffer },
															VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
															modelBufferOffset);

	// Copy data to uniform memory 
	{
		char* data;
		vkMapMemory(device->GetVkDevice(), modelBufferMemory, 0, sizeof(ModelUBO), 0, reinterpret_cast<void**>(&data));// reinterpret_cast = Leave data as bits, don't reformat it
		memcpy(data, &modelTransforms, sizeof(ModelUBO));
		vkUnmapMemory(device->GetVkDevice(), modelBufferMemory);
	}

	// Bind memory to buffers
	BufferUtils::BindMemoryForBuffers(device, { modelBuffer }, modelBufferMemory, modelBufferOffset);

	CreateAndFillBufferResources(vertexBuffer, vertexBufferSize, indexBuffer, indexBufferSize);

	// Create cloud textures
	CreateCloudTextureResources(textureImage, textureImageMemory, textureImageView, textureSampler);

	//Write to and Update DescriptorSets
	WriteToAndUpdateDescriptorSets(vertexBuffer, vertexBufferSize, modelBuffer, textureImageView, textureSampler);
}

void Renderer::CreateAndFillBufferResources(VkBuffer& vertexBuffer, unsigned int& vertexBufferSize, 
											VkBuffer& indexBuffer, unsigned int& indexBufferSize)
{
	// Create vertices and indices vectors to bind to buffers
	vertices = {
		{ { -0.5f, 0.5f,  0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
		{ { 0.5f,  0.5f,  0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
		{ { 0.5f,  -0.5f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
		{ { -0.5f, -0.5f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } },

		{ { -0.5f, 0.5f,  -0.5f, 1.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
		{ { 0.5f,  0.5f,  -0.5f, 1.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
		{ { 0.5f,  -0.5f, -0.5f, 1.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
		{ { -0.5f, -0.5f, -0.5f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } }
	};

	std::vector<unsigned int> indices = { 0, 1, 2, 2, 3, 0,
										  4, 5, 6, 6, 7, 4 };

	vertexBufferSize = static_cast<uint32_t>(vertices.size() * sizeof(vertices[0]));
	indexBufferSize = static_cast<uint32_t>(indices.size() * sizeof(indices[0]));

	// Create vertex and index buffers
	vertexBuffer = BufferUtils::CreateBuffer(device, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, vertexBufferSize);
	indexBuffer = BufferUtils::CreateBuffer(device, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBufferSize);
	unsigned int vertexBufferOffsets[2];

	// VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT bit indicates that memory allocated with this type can be mapped for host access using vkMapMemory
	// VK_MEMORY_PROPERTY_HOST_COHERENT_BIT allows to transfer host writes to the device or make device writes visible to the host (without needing other cache mgmt commands)
	vertexBufferMemory = BufferUtils::AllocateMemoryForBuffers(device,
															{ vertexBuffer, indexBuffer },
															VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
															vertexBufferOffsets);

	// Copy data to buffer memory
	{
		char* data;
		// The size is equal to vertexBufferOffsets + indexBufferSize 
		// vertexBufferOffsets stores the amount of memory and is calculated in the call to AllocateMemoryForBuffers above
		vkMapMemory(device->GetVkDevice(), vertexBufferMemory, 0, vertexBufferOffsets[1] + indexBufferSize, 0, reinterpret_cast<void**>(&data));
		memcpy(data + vertexBufferOffsets[0], vertices.data(), vertexBufferSize);
		memcpy(data + vertexBufferOffsets[1], indices.data(), indexBufferSize);
		vkUnmapMemory(device->GetVkDevice(), vertexBufferMemory);
	}

	// Bind the memory to the buffers
	BufferUtils::BindMemoryForBuffers(device, { vertexBuffer, indexBuffer }, vertexBufferMemory, vertexBufferOffsets);
}

void Renderer::CreateCloudTextureResources(VkImage& textureImage, VkDeviceMemory& textureImageMemory, VkImageView& textureImageView, VkSampler& textureSampler)
{
	Image::loadTexture(device, graphicsCommandPool, "../../src/CloudScapes/textures/statue.jpg", textureImage, textureImageMemory,
		VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	Image::createImageView(device, textureImageView, textureImage,
		VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

	Image::createSampler(device, textureSampler);
}

void Renderer::WriteToAndUpdateDescriptorSets(VkBuffer& vertexBuffer, unsigned int& vertexBufferSize, VkBuffer& modelBuffer, 
											  VkImageView& textureImageView, VkSampler& textureSampler)
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
	cameraBufferInfo.buffer = camera->GetBuffer();
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

	vkUpdateDescriptorSets(device->GetVkDevice(), 4, writeDescriptorSets, 0, nullptr);
}

//----------------------------------------------
//-------------- Format Helper Functions -------
//----------------------------------------------
VkFormat Renderer::findSupportedFormat(VkPhysicalDevice physicalDevice,
									   const std::vector<VkFormat>& candidates,
									   VkImageTiling tiling, VkFormatFeatureFlags features)
{
	for (VkFormat format : candidates)
	{
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}

	throw std::runtime_error("failed to find supported format!");
}

VkFormat Renderer::findDepthFormat()
{
	return VK_FORMAT_D24_UNORM_S8_UINT;

	//TODO
	//If you want to get sophisticated and fins a depth format compatible with any GPU do the checks below:
	/*
	return findSupportedFormat(
	physicalDevice,
	{ VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT },
	VK_IMAGE_TILING_OPTIMAL,
	VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
	*/
}
