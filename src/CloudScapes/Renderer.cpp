#include "Renderer.h"

Renderer::Renderer(VulkanDevice* device, VkPhysicalDevice physicalDevice, VulkanSwapChain* swapChain, 
	Scene* scene, Sky* sky, Camera* camera, Camera* cameraOld, uint32_t width, uint32_t height)
	: device(device),
	logicalDevice(device->GetVkDevice()),
	physicalDevice(physicalDevice),
	swapChain(swapChain),
	scene(scene),
	sky(sky),
	camera(camera),
	cameraOld(cameraOld),
	window_width(width),
	window_height(height)
{
	InitializeRenderer();
}

Renderer::~Renderer()
{
	DestroyOnWindowResize(); //Destroys a lot of things already --> so why write it again

	//Command Pools
	vkDestroyCommandPool(logicalDevice, graphicsCommandPool, nullptr);
	vkDestroyCommandPool(logicalDevice, computeCommandPool, nullptr);
	
	//Descriptor Set Layouts
	vkDestroyDescriptorSetLayout(logicalDevice, cloudComputeSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, graphicsSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, pingPongCloudResultSetLayout, nullptr);

	vkDestroyDescriptorSetLayout(logicalDevice, cameraSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, timeSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, sunAndSkySetLayout, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, keyPressQuerySetLayout, nullptr);

	vkDestroyDescriptorSetLayout(logicalDevice, godRaysSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, toneMapSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, TXAASetLayout, nullptr);

	//Descriptor Set
	vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);

	//Cloud and sky resources that are independent of size
	delete sky;
}

void Renderer::DestroyOnWindowResize()
{
	vkDeviceWaitIdle(logicalDevice);

	vkFreeCommandBuffers(logicalDevice, graphicsCommandPool, static_cast<uint32_t>(graphicsCommandBuffer1.size()), graphicsCommandBuffer1.data());
	vkFreeCommandBuffers(logicalDevice, graphicsCommandPool, static_cast<uint32_t>(graphicsCommandBuffer2.size()), graphicsCommandBuffer2.data());
	vkFreeCommandBuffers(logicalDevice, computeCommandPool, 1, &computeCommandBuffer1);
	vkFreeCommandBuffers(logicalDevice, computeCommandPool, 1, &computeCommandBuffer2);

	DestroyFrameResources();

	//Regular Pipelines
	// All 3 pipelines have things that depend on the window width and height and so we need to recreate all of those resources when resizing
	//This Function recreates the frame resources which in turn means we need to recreate the graphics pipeline and rerecord the graphics command buffers
	vkDestroyPipelineLayout(logicalDevice, graphicsPipelineLayout, nullptr);
	vkDestroyPipelineLayout(logicalDevice, cloudComputePipelineLayout, nullptr);
	vkDestroyPipelineLayout(logicalDevice, reprojectionPipelineLayout, nullptr);
	vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);
	vkDestroyPipeline(logicalDevice, cloudComputePipeline, nullptr);
	vkDestroyPipeline(logicalDevice, reprojectionPipeline, nullptr);

	//Post Process Pipelines
	vkDestroyPipelineCache(logicalDevice, postProcessPipeLineCache, nullptr);
	vkDestroyPipelineLayout(logicalDevice, postProcess_GodRays_PipelineLayout, nullptr);
	vkDestroyPipelineLayout(logicalDevice, postProcess_ToneMap_PipelineLayout, nullptr);
	vkDestroyPipelineLayout(logicalDevice, postProcess_TXAA_PipelineLayout, nullptr);
	vkDestroyPipeline(logicalDevice, postProcess_GodRays_PipeLine, nullptr);
	vkDestroyPipeline(logicalDevice, postProcess_ToneMap_PipeLine, nullptr);
	vkDestroyPipeline(logicalDevice, postProcess_TXAA_PipeLine, nullptr);

	//Render Pass
	vkDestroyRenderPass(logicalDevice, renderPass, nullptr);

	//Textures
	delete currentFrameTexture;
	delete previousFrameTexture;
	delete currentCloudsResultTexture;
	delete previousCloudsResultTexture;
	delete godRaysCreationDataTexture;
}

void Renderer::InitializeRenderer()
{
	VulkanInitializers::CreateCommandPool(logicalDevice, graphicsCommandPool, device->GetInstance()->GetQueueFamilyIndices()[QueueFlags::Graphics] );
	VulkanInitializers::CreateCommandPool(logicalDevice, computeCommandPool, device->GetInstance()->GetQueueFamilyIndices()[QueueFlags::Compute] );

	CreateRenderPass();

	CreateResources();
	sky->CreateCloudResources(computeCommandPool);

	CreateDescriptorPool();
	CreateAllDescriptorSetLayouts();
	CreateAllDescriptorSets();

	CreateFrameResources();

	CreateAllPipeLines(renderPass, 0);
	RecordAllCommandBuffers();
}

void Renderer::RecreateOnResize(uint32_t width, uint32_t height)
{
	window_width = width;
	window_height = height;

	RecreateFrameResources();
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
	if (swapPingPongBuffers) {
		computeSubmitInfo.pCommandBuffers = &computeCommandBuffer1;
	}
	else {
		computeSubmitInfo.pCommandBuffers = &computeCommandBuffer2;
	}

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
	if (swapPingPongBuffers) {
		graphicsSubmitInfo.pCommandBuffers = &graphicsCommandBuffer1[swapChain->GetIndex()];
	}
	else {
		graphicsSubmitInfo.pCommandBuffers = &graphicsCommandBuffer2[swapChain->GetIndex()];
	}	

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

	swapPingPongBuffers = !swapPingPongBuffers;
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
	depthAttachment.format = FormatUtils::FindDepthFormat(physicalDevice); //The format should be the same as the depth image itself.
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
void Renderer::CreateAllPipeLines(VkRenderPass renderPass, unsigned int subpass)
{
	cloudComputePipelineLayout = VulkanInitializers::CreatePipelineLayout( logicalDevice, { pingPongCloudResultSetLayout, 
																						cloudComputeSetLayout, 
																						cameraSetLayout, 
																						timeSetLayout, 
																						sunAndSkySetLayout, 
																						keyPressQuerySetLayout });
	reprojectionPipelineLayout = VulkanInitializers::CreatePipelineLayout( logicalDevice, { pingPongCloudResultSetLayout,	
																							cameraSetLayout, 
																							cameraSetLayout,
																							timeSetLayout });
	graphicsPipelineLayout = VulkanInitializers::CreatePipelineLayout( logicalDevice, { graphicsSetLayout, 
																						cameraSetLayout });	
	postProcess_GodRays_PipelineLayout = VulkanInitializers::CreatePipelineLayout( logicalDevice, { pingPongCloudResultSetLayout, 
																									godRaysSetLayout, 
																									cameraSetLayout, 
																									sunAndSkySetLayout });
	postProcess_ToneMap_PipelineLayout = VulkanInitializers::CreatePipelineLayout( logicalDevice, { toneMapSetLayout });
	postProcess_TXAA_PipelineLayout = VulkanInitializers::CreatePipelineLayout(logicalDevice, { toneMapSetLayout });
	
	CreateComputePipeline(cloudComputePipelineLayout, cloudComputePipeline, "CloudScapes/shaders/cloudRayMarch.comp.spv");
	CreateComputePipeline(reprojectionPipelineLayout, reprojectionPipeline, "CloudScapes/shaders/reprojection.comp.spv");
	CreateGraphicsPipeline(renderPass, 0);
	CreatePostProcessPipeLines(renderPass);
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
	VkShaderModule vertShaderModule = ShaderModule::createShaderModule("CloudScapes/shaders/geometryPlain.vert.spv", device->GetVkDevice());
	VkShaderModule fragShaderModule = ShaderModule::createShaderModule("CloudScapes/shaders/geometryPlain.frag.spv", device->GetVkDevice());

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
	VkPipelineVertexInputStateCreateInfo vertexInputState = 
		VulkanInitializers::pipelineVertexInputStateCreateInfo();
	vertexInputState.vertexBindingDescriptionCount = 1;
	vertexInputState.pVertexBindingDescriptions = &vertexInputBinding;
	vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
	vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

	// -------- Input assembly --------
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
		VulkanInitializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);

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

	VkPipelineViewportStateCreateInfo viewportState =
		VulkanInitializers::pipelineViewportStateCreateInfo(1, 1, 0);

	viewportState.pViewports = &viewport;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizationState =
		VulkanInitializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE, 0);

	VkPipelineMultisampleStateCreateInfo multisamplingState =
		VulkanInitializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);

	VkPipelineDepthStencilStateCreateInfo depthStencilState =
		VulkanInitializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS);

	VkColorComponentFlags colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	VkPipelineColorBlendAttachmentState blendAttachmentState =
		VulkanInitializers::pipelineColorBlendAttachmentState(colorWriteMask, VK_FALSE);

	VkPipelineColorBlendStateCreateInfo colorBlendingState =
		VulkanInitializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

	// -------- Create graphics pipeline ---------
	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2; //vert and frag --> change if you add more
	pipelineInfo.pStages = shaderStages; //defined above
	pipelineInfo.pVertexInputState = &vertexInputState; //defined above
	pipelineInfo.pInputAssemblyState = &inputAssemblyState; //defined above
	pipelineInfo.pViewportState = &viewportState; //defined above
	pipelineInfo.pRasterizationState = &rasterizationState; //defined above
	pipelineInfo.pMultisampleState = &multisamplingState; //defined above
	pipelineInfo.pDepthStencilState = &depthStencilState; //defined above
	pipelineInfo.pColorBlendState = &colorBlendingState; //defined above
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
void Renderer::CreateComputePipeline(VkPipelineLayout& _computePipelineLayout, VkPipeline& _computePipeline, const std::string &filename)
{
	VkShaderModule compShaderModule = ShaderModule::createShaderModule(filename, device->GetVkDevice());

	VkPipelineShaderStageCreateInfo compShaderStageInfo = {};
	compShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	compShaderStageInfo.module = compShaderModule;
	compShaderStageInfo.pName = "main";

	VkComputePipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.stage = compShaderStageInfo;
	pipelineInfo.layout = _computePipelineLayout;

	if (vkCreateComputePipelines(device->GetVkDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_computePipeline) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create pipeline");
	}

	vkDestroyShaderModule(device->GetVkDevice(), compShaderModule, nullptr);
}
void Renderer::CreatePostProcessPipeLines(VkRenderPass renderPass)
{
	// -------- Vertex input binding --------
	VkPipelineVertexInputStateCreateInfo emptyVertexInputState = VulkanInitializers::pipelineVertexInputStateCreateInfo();

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
		VulkanInitializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);

	VkPipelineRasterizationStateCreateInfo rasterizationState =
		VulkanInitializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);

	VkPipelineColorBlendAttachmentState blendAttachmentState =
		VulkanInitializers::pipelineColorBlendAttachmentState(0x0, VK_FALSE);

	VkPipelineColorBlendStateCreateInfo colorBlendState =
		VulkanInitializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

	VkPipelineDepthStencilStateCreateInfo depthStencilState =
		VulkanInitializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);

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

	VkPipelineViewportStateCreateInfo viewportState =
		VulkanInitializers::pipelineViewportStateCreateInfo(1, 1, 0);

	viewportState.pViewports = &viewport;
	viewportState.pScissors = &scissor;

	VkPipelineMultisampleStateCreateInfo multiSampleState =
		VulkanInitializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);

	// No Dynamic states
	//std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	//VkPipelineDynamicStateCreateInfo dynamicState =
	//	VulkanInitializers::pipelineDynamicStateCreateInfo( dynamicStateEnables, 0 );

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages; //Defined later on and changes per pipeline binding since we are creating many pipelines here

	// -------- Create Base PostProcess pipeline Info ---------
	VkGraphicsPipelineCreateInfo postProcessPipelineCreateInfo =
		VulkanInitializers::graphicsPipelineCreateInfo(postProcess_GodRays_PipelineLayout, renderPass, 0);

	postProcessPipelineCreateInfo.pVertexInputState = &emptyVertexInputState;; //defined above
	postProcessPipelineCreateInfo.pInputAssemblyState = &inputAssemblyState; //defined above
	postProcessPipelineCreateInfo.pRasterizationState = &rasterizationState; //defined above
	postProcessPipelineCreateInfo.pColorBlendState = &colorBlendState; //defined above
	postProcessPipelineCreateInfo.pMultisampleState = &multiSampleState; //defined above
	postProcessPipelineCreateInfo.pViewportState = &viewportState; //defined above
	postProcessPipelineCreateInfo.pDepthStencilState = &depthStencilState; //defined above
	postProcessPipelineCreateInfo.pDynamicState = nullptr; //defined above
	postProcessPipelineCreateInfo.subpass = 0; // no subpasses
	postProcessPipelineCreateInfo.stageCount = shaderStages.size(); //reserving memory for the shader stages that will soon be defined
	postProcessPipelineCreateInfo.pStages = shaderStages.data();

	// -------- Create Multiple Pipelines based on the above base ---------
	//Create a pipeline cache so multiple pieplines cane be created from the same pipeline creation Info
	VulkanInitializers::createPipelineCache(logicalDevice, postProcessPipeLineCache);

	VkShaderModule generic_vertShaderModule =
		ShaderModule::createShaderModule("CloudScapes/shaders/postProcess_GenericVertShader.vert.spv", logicalDevice);
	shaderStages[0] = VulkanInitializers::loadShader(VK_SHADER_STAGE_VERTEX_BIT, generic_vertShaderModule);

	// -------- God Rays pipeline -----------------------------------------
	VkShaderModule godRays_fragShaderModule =
		ShaderModule::createShaderModule("CloudScapes/shaders/postProcess_GodRays.frag.spv", logicalDevice);

	// Assign each shader module to the appropriate stage in the pipeline
	shaderStages[1] = VulkanInitializers::loadShader(VK_SHADER_STAGE_FRAGMENT_BIT, godRays_fragShaderModule);

	postProcessPipelineCreateInfo.layout = postProcess_GodRays_PipelineLayout;

	if (vkCreateGraphicsPipelines(logicalDevice, postProcessPipeLineCache, 1, &postProcessPipelineCreateInfo, nullptr, &postProcess_GodRays_PipeLine) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create post process pipeline");
	}

	vkDestroyShaderModule(device->GetVkDevice(), godRays_fragShaderModule, nullptr);

	// -------- Tone Map Post -----------------------------------------
	VkShaderModule toneMap_fragShaderModule =
		ShaderModule::createShaderModule("CloudScapes/shaders/postProcess_ToneMap.frag.spv", logicalDevice);

	// Assign each shader module to the appropriate stage in the pipeline
	shaderStages[1] = VulkanInitializers::loadShader(VK_SHADER_STAGE_FRAGMENT_BIT, toneMap_fragShaderModule);

	postProcessPipelineCreateInfo.layout = postProcess_ToneMap_PipelineLayout;

	if (vkCreateGraphicsPipelines(logicalDevice, postProcessPipeLineCache, 1, &postProcessPipelineCreateInfo, nullptr, &postProcess_ToneMap_PipeLine) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create post process pipeline");
	}

	vkDestroyShaderModule(device->GetVkDevice(), toneMap_fragShaderModule, nullptr);

	// -------- Enable Writing to Frame Buffer Again ------------------
	// BlendAttachmentState in addition to defining the blend state also defines if the shaders can write to the framebuffer with the color write mask
	// Color Write Mask Reference: https://www.khronos.org/registry/vulkan/specs/1.0/man/html/VkColorComponentFlagBits.html
	blendAttachmentState = VulkanInitializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);

	// -------- Anti Aliasing  pipeline -----------------------------------------
	VkShaderModule TXAA_fragShaderModule =
		ShaderModule::createShaderModule("CloudScapes/shaders/postProcess_TXAA.frag.spv", logicalDevice);

	// Assign each shader module to the appropriate stage in the pipeline
	shaderStages[1] = VulkanInitializers::loadShader(VK_SHADER_STAGE_FRAGMENT_BIT, TXAA_fragShaderModule);

	postProcessPipelineCreateInfo.layout = postProcess_TXAA_PipelineLayout;

	if (vkCreateGraphicsPipelines(logicalDevice, postProcessPipeLineCache, 1, &postProcessPipelineCreateInfo, nullptr, &postProcess_TXAA_PipeLine) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create post process pipeline");
	}

	vkDestroyShaderModule(device->GetVkDevice(), TXAA_fragShaderModule, nullptr);
	vkDestroyShaderModule(device->GetVkDevice(), generic_vertShaderModule, nullptr);
}

//----------------------------------------------
//-------------- Frame Resources ---------------
//----------------------------------------------
void Renderer::CreateFrameResources()
{
	// Create the depth image and imageView that needs to be attached to the frame buffer
	VkFormat depthFormat = FormatUtils::FindDepthFormat(physicalDevice);
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

	CreateFrameBuffers(renderPass);
}
void Renderer::DestroyFrameResources()
{
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
	DestroyOnWindowResize();

	CreateResources();
	CreateRenderPass();

	WriteToAndUpdateAllDescriptorSets();

	CreateFrameResources();
	CreateAllPipeLines(renderPass, 0);

	RecordAllCommandBuffers();
}

void Renderer::CreateFrameBuffers(VkRenderPass renderPass)
{
	frameBuffers.resize(swapChain->GetCount());
	for (uint32_t i = 0; i < swapChain->GetCount(); i++)
	{
		std::array<VkImageView, 2> attachments = { swapChain->GetVkImageView(i), depthImageView };

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
//-------------- Command Buffers ---------------
//----------------------------------------------
void Renderer::RecordAllCommandBuffers()
{
	VkImage currFrameImage = currentCloudsResultTexture->GetTextureImage();
	VkImage prevFrameImage = previousCloudsResultTexture->GetTextureImage();

	RecordComputeCommandBuffer(computeCommandBuffer1, pingPongCloudResultSet1);
	RecordGraphicsCommandBuffer(graphicsCommandBuffer1, currFrameImage, pingPongCloudResultSet1, toneMapSet1, TXAASet1);

	RecordComputeCommandBuffer(computeCommandBuffer2, pingPongCloudResultSet2);
	RecordGraphicsCommandBuffer(graphicsCommandBuffer2, prevFrameImage, pingPongCloudResultSet2, toneMapSet2, TXAASet2);
}
void Renderer::RecordComputeCommandBuffer(VkCommandBuffer &computeCmdBuffer, VkDescriptorSet& pingPongFrameSet)
{
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.commandPool = computeCommandPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = 1;

	if (vkAllocateCommandBuffers(logicalDevice, &commandBufferAllocateInfo, &computeCmdBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate compute command buffers");
	}

	// Record command buffer-> should not nead one for every frame of the swapchain
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT; // How we are using the command buffer
																	// The command buffer can be resubmitted while it is also already pending execution.
	beginInfo.pInheritanceInfo = nullptr; //only useful for secondary command buffers

										  //---------- Begin recording ----------
										  //If the command buffer was already recorded once, then a call to vkBeginCommandBuffer will implicitly reset it. 
										  // It's not possible to append commands to a buffer at a later time.
	if (vkBeginCommandBuffer(computeCmdBuffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("Failed to begin recording compute command buffer");
	}

	//-----------------------------------------------------
	//--- Compute Pipeline Binding, Dispatch & Barriers ---
	//-----------------------------------------------------
	uint32_t numBlocksX = (window_width + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
	uint32_t numBlocksY = (window_height + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
	uint32_t numBlocksZ = 1;

	//Bind the compute piepline
	vkCmdBindPipeline(computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, reprojectionPipeline);

	//Bind Descriptor Sets for compute
	vkCmdBindDescriptorSets(computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, reprojectionPipelineLayout, 0, 1, &pingPongFrameSet, 0, nullptr);
	vkCmdBindDescriptorSets(computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, reprojectionPipelineLayout, 1, 1, &cameraSet, 0, nullptr);
	vkCmdBindDescriptorSets(computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, reprojectionPipelineLayout, 2, 1, &cameraOldSet, 0, nullptr);
	vkCmdBindDescriptorSets(computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, reprojectionPipelineLayout, 3, 1, &timeSet, 0, nullptr);

	// Dispatch the compute kernel
	// similar to a kernel call --> void vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);	
	vkCmdDispatch(computeCmdBuffer, numBlocksX, numBlocksY, numBlocksZ);

	//Bind Descriptor Sets for compute
	vkCmdBindDescriptorSets(computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, cloudComputePipelineLayout, 0, 1, &pingPongFrameSet, 0, nullptr);
	vkCmdBindDescriptorSets(computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, cloudComputePipelineLayout, 1, 1, &cloudComputeSet, 0, nullptr);
	vkCmdBindDescriptorSets(computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, cloudComputePipelineLayout, 2, 1, &cameraSet, 0, nullptr);
	vkCmdBindDescriptorSets(computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, cloudComputePipelineLayout, 3, 1, &timeSet, 0, nullptr);
	vkCmdBindDescriptorSets(computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, cloudComputePipelineLayout, 4, 1, &sunAndSkySet, 0, nullptr);
	vkCmdBindDescriptorSets(computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, cloudComputePipelineLayout, 5, 1, &keyPressQuerySet, 0, nullptr);

	//Bind the compute piepline
	vkCmdBindPipeline(computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, cloudComputePipeline);

	// Dispatch the compute kernel
	// similar to a kernel call --> void vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);	
	numBlocksX = (std::ceil(window_width / 4) + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
	numBlocksY = (std::ceil(window_height / 4) + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;

	vkCmdDispatch(computeCmdBuffer, numBlocksX, numBlocksY, numBlocksZ);

	//---------- End Recording ----------
	if (vkEndCommandBuffer(computeCmdBuffer) != VK_SUCCESS) {
		throw std::runtime_error("Failed to record the compute command buffer");
	}
}
void Renderer::RecordGraphicsCommandBuffer(std::vector<VkCommandBuffer> &graphicsCmdBuffer, VkImage &Image_for_barrier, 
											VkDescriptorSet& pingPongCloudResultSet, VkDescriptorSet& toneMapSet, VkDescriptorSet& TXAASet)
{
	graphicsCmdBuffer.resize(swapChain->GetCount());

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
	commandBufferAllocateInfo.commandBufferCount = (uint32_t)(graphicsCmdBuffer.size());

	if (vkAllocateCommandBuffers(logicalDevice, &commandBufferAllocateInfo, graphicsCmdBuffer.data()) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate graphics command buffers");
	}

	// Record graphics command buffers, one for each frame of the swapchain
	for (unsigned int i = 0; i < graphicsCmdBuffer.size(); ++i)
	{
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT; // How we are using the command buffer
																		// The command buffer can be resubmitted while it is also already pending execution.
		beginInfo.pInheritanceInfo = nullptr; //only useful for secondary command buffers

		//---------- Begin recording ----------
		//If the command buffer was already recorded once, then a call to vkBeginCommandBuffer will implicitly reset it. 
		// It's not possible to append commands to a buffer at a later time.
		if (vkBeginCommandBuffer(graphicsCmdBuffer[i], &beginInfo) != VK_SUCCESS) {
			throw std::runtime_error("Failed to begin recording graphics command buffer");
		}

		// Begin the render pass
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

		//---------------------------------------------------------
		//--- Graphics and Clouds Pipeline Binding and Dispatch ---
		//---------------------------------------------------------
		// Define a memory barrier to transition the vertex buffer from a compute storage object to a vertex input
		// Each element of the pMemoryBarriers, pBufferMemoryBarriers and pImageMemoryBarriers arrays specifies two halves of a memory dependency, as defined above.
		// Reference: https://vulkan.lunarg.com/doc/view/1.0.30.0/linux/vkspec.chunked/ch06s05.html#synchronization-memory-barriers
		VkImageMemoryBarrier imageMemoryBarrier = {};
		imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageMemoryBarrier.image = Image_for_barrier;
		imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		// A pipeline barrier inserts an execution dependency and a set of memory dependencies between a set of commands earlier in the command buffer and a set of commands later in the command buffer.
		// Reference: https://vulkan.lunarg.com/doc/view/1.0.30.0/linux/vkspec.chunked/ch06s05.html
		vkCmdPipelineBarrier(graphicsCmdBuffer[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

		vkCmdBeginRenderPass(graphicsCmdBuffer[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		// VK_SUBPASS_CONTENTS_INLINE: The render pass commands will be embedded in the primary command
		// buffer itself and no secondary command buffers will be executed.

		//------------------------
		//--- Graphics Pipeline---
		//------------------------
		/*
		// Uncomment for models :D --> except we can only load small obj's at the moment
		
		// Bind the graphics pipeline
		vkCmdBindPipeline(graphicsCommandBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		// Bind graphics descriptor set
		vkCmdBindDescriptorSets(graphicsCommandBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout, 0, 1, &graphicsSet, 0, nullptr);
		vkCmdBindDescriptorSets(graphicsCommandBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout, 1, 1, &cameraSet, 0, nullptr);

		// Bind the vertex and index buffers
		VkDeviceSize geomOffsets[] = { 0 };
		const VkBuffer geomVertices = scene->GetModels()[0]->getVertexBuffer();
		vkCmdBindVertexBuffers(graphicsCommandBuffer[i], 0, 1, &geomVertices, geomOffsets);
		vkCmdBindIndexBuffer(graphicsCommandBuffer[i], scene->GetModels()[0]->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

		// Draw indexed triangle
		vkCmdDrawIndexed(graphicsCommandBuffer[i], scene->GetModels()[0]->getIndexBufferSize(), 1, 0, 0, 1);
		*/
		
		//-----------------------------
		//--- PostProcess Pipelines ---
		//-----------------------------
		// God Rays Pipeline
		//vkCmdBindDescriptorSets(graphicsCmdBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, postProcess_GodRays_PipelineLayout, 0, 1, &pingPongCloudResultSet, 0, NULL);
		//vkCmdBindDescriptorSets(graphicsCmdBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, postProcess_GodRays_PipelineLayout, 1, 1, &godRaysSet, 0, NULL);
		//vkCmdBindDescriptorSets(graphicsCmdBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, postProcess_GodRays_PipelineLayout, 2, 1, &cameraSet, 0, NULL);
		//vkCmdBindDescriptorSets(graphicsCmdBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, postProcess_GodRays_PipelineLayout, 3, 1, &sunAndSkySet, 0, NULL);
		//vkCmdBindPipeline(graphicsCmdBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, postProcess_GodRays_PipeLine);
		//vkCmdDraw(graphicsCmdBuffer[i], 3, 1, 0, 0);

		// Tone Map Pass Pipeline
		vkCmdBindDescriptorSets(graphicsCmdBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, postProcess_ToneMap_PipelineLayout, 0, 1, &toneMapSet, 0, NULL);
		vkCmdBindPipeline(graphicsCmdBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, postProcess_ToneMap_PipeLine);
		vkCmdDraw(graphicsCmdBuffer[i], 3, 1, 0, 0);

		// Temporal Anti-Aliasing Pass Pipeline
		vkCmdBindDescriptorSets(graphicsCmdBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, postProcess_TXAA_PipelineLayout, 0, 1, &TXAASet, 0, NULL);
		vkCmdBindPipeline(graphicsCmdBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, postProcess_TXAA_PipeLine);
		vkCmdDraw(graphicsCmdBuffer[i], 3, 1, 0, 0);

		//---------- End RenderPass ---------
		vkCmdEndRenderPass(graphicsCmdBuffer[i]);

		//---------- End Recording ----------
		if (vkEndCommandBuffer(graphicsCmdBuffer[i]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to record the graphics command buffer");
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
		// ------------ Curr and Prev Cloud Results -----------------
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }, // Current Cloud Result
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }, // Previous Cloud Result
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }, // Current Cloud Result
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }, // Previous Cloud Result
		// ------------ Compute ------------------------------
		// Samplers for all the cloud Textures
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }, // low frequency texture
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }, // high frequency texture
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }, // curl noise texture
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }, // Weather Map
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }, //God Rays Mask

		// ------------ Graphics -----------------------------
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }, //model matrix
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }, //texture sampler for model

		// -------- Can be attached to multiple pipelines ------
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }, // Camera
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }, // previous Frame Camera
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }, // Time
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }, // SunAndSky
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }, // KeyPress

		// ------------ PostProcess pipelines -----------------
		// GodRays -- GreyScale Image of where light is in the sky
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
		
		// Tone Map Pass (2 sets --> curr and prev pingponged cloud results)
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },

		// Anti Aliasing  (2 sets --> curr and prev pingponged frames)
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
	};
	
	VulkanInitializers::CreateDescriptorPool(logicalDevice, static_cast<uint32_t>(poolSizes.size()), poolSizes.data(), descriptorPool);
}

void Renderer::CreateAllDescriptorSetLayouts()
{
	// Create the array of VkDescriptorSetLayoutBinding's 
	// Binding --> used in shader
	// Descriptor Type --> type of descriptor
	// Descriptor Count --> Shader variable can represent an array of UBO's, descriptorCount specifies number of values in the array
	// Stage Flags --> which shader you're referencing this descriptor from 
	// pImmutableSamplers --> for image sampling related descriptors
	
	// Ping Pong Set 1
	VkDescriptorSetLayoutBinding currentCloudResultLayoutBinding = { 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_ALL, nullptr };
	VkDescriptorSetLayoutBinding previousCloudResultLayoutBinding = { 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_ALL, nullptr };
	std::array<VkDescriptorSetLayoutBinding, 2> pingPongFrameBindings = { currentCloudResultLayoutBinding, previousCloudResultLayoutBinding };
	VulkanInitializers::CreateDescriptorSetLayout(logicalDevice, static_cast<uint32_t>(pingPongFrameBindings.size()), pingPongFrameBindings.data(), pingPongCloudResultSetLayout);
	
	//-------------------- Computes Pipeline --------------------
	VkDescriptorSetLayoutBinding cloudLowFrequencyNoiseSetLayoutBinding = { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
	VkDescriptorSetLayoutBinding cloudHighFrequencyNoiseSetLayoutBinding = { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
	VkDescriptorSetLayoutBinding cloudCurlNoiseSetLayoutBinding = { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
	VkDescriptorSetLayoutBinding weatherMapSetLayoutBinding = { 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
	VkDescriptorSetLayoutBinding godRaysCreationDataSetLayoutBinding = { 4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };

	std::array<VkDescriptorSetLayoutBinding, 5> cloudRayMarchBindings = { cloudLowFrequencyNoiseSetLayoutBinding, cloudHighFrequencyNoiseSetLayoutBinding,
																cloudCurlNoiseSetLayoutBinding, weatherMapSetLayoutBinding, godRaysCreationDataSetLayoutBinding };
	VulkanInitializers::CreateDescriptorSetLayout(logicalDevice, static_cast<uint32_t>(cloudRayMarchBindings.size()), cloudRayMarchBindings.data(), cloudComputeSetLayout);

	//-------------------- Graphics Pipeline --------------------
	VkDescriptorSetLayoutBinding modelSetLayoutBinding = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr };
	VkDescriptorSetLayoutBinding samplerSetLayoutBinding = { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };

	std::array<VkDescriptorSetLayoutBinding, 2> graphicsBindings = { modelSetLayoutBinding, samplerSetLayoutBinding };
	VulkanInitializers::CreateDescriptorSetLayout(logicalDevice, static_cast<uint32_t>(graphicsBindings.size()), graphicsBindings.data(), graphicsSetLayout);

	//-------------------- Descriptors that are discrete and mey be attached to multiple pieplines --------------------
	//Camera
	VkDescriptorSetLayoutBinding cameraSetLayoutBinding = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr };

	std::array<VkDescriptorSetLayoutBinding, 1> cameraBindings = { cameraSetLayoutBinding };
	VulkanInitializers::CreateDescriptorSetLayout(logicalDevice, static_cast<uint32_t>(cameraBindings.size()), cameraBindings.data(), cameraSetLayout);

	//Time --> Would be better to use pushConstants --> TODO at somepoint
	VkDescriptorSetLayoutBinding timeSetLayoutBinding = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr };

	std::array<VkDescriptorSetLayoutBinding, 1> timeBindings = { timeSetLayoutBinding };
	VulkanInitializers::CreateDescriptorSetLayout(logicalDevice, static_cast<uint32_t>(timeBindings.size()), timeBindings.data(), timeSetLayout);

	//SunAndSky
	VkDescriptorSetLayoutBinding sunAndSkySetLayoutBinding = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr };

	std::array<VkDescriptorSetLayoutBinding, 1> sunAndSkyBindings = { sunAndSkySetLayoutBinding };
	VulkanInitializers::CreateDescriptorSetLayout(logicalDevice, static_cast<uint32_t>(sunAndSkyBindings.size()), sunAndSkyBindings.data(), sunAndSkySetLayout);

	//KeyPressQuery
	VkDescriptorSetLayoutBinding keyPressQuerySetLayoutBinding = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr };

	std::array<VkDescriptorSetLayoutBinding, 1> keyPressQueryBindings = { keyPressQuerySetLayoutBinding };
	VulkanInitializers::CreateDescriptorSetLayout(logicalDevice, static_cast<uint32_t>(keyPressQueryBindings.size()), keyPressQueryBindings.data(), keyPressQuerySetLayout);

	//-------------------- Post Process --------------------
	//God Rays
	VkDescriptorSetLayoutBinding godRaysSetLayoutBinding = { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };

	std::array<VkDescriptorSetLayoutBinding, 1> godRaysBindings = { godRaysSetLayoutBinding };
	VulkanInitializers::CreateDescriptorSetLayout(logicalDevice, static_cast<uint32_t>(godRaysBindings.size()), godRaysBindings.data(), godRaysSetLayout);

	//Tone Map Pass
	VkDescriptorSetLayoutBinding toneMapInputImageSetLayoutBinding = { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };
	VkDescriptorSetLayoutBinding toneMapWriteImageSetLayoutBinding = { 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };

	std::array<VkDescriptorSetLayoutBinding, 2> toneMapBindings = { toneMapInputImageSetLayoutBinding, toneMapWriteImageSetLayoutBinding };
	VulkanInitializers::CreateDescriptorSetLayout(logicalDevice, static_cast<uint32_t>(toneMapBindings.size()), toneMapBindings.data(), toneMapSetLayout);

	//TXAA Pass
	VkDescriptorSetLayoutBinding TXAAPrevFrameSetLayoutBinding = { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };
	VkDescriptorSetLayoutBinding TXAACurrentFrameSetLayoutBinding = { 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };

	std::array<VkDescriptorSetLayoutBinding, 2> TXAABindings = { TXAAPrevFrameSetLayoutBinding, TXAACurrentFrameSetLayoutBinding };
	VulkanInitializers::CreateDescriptorSetLayout(logicalDevice, static_cast<uint32_t>(TXAABindings.size()), TXAABindings.data(), TXAASetLayout);
}
void Renderer::CreateAllDescriptorSets()
{
	// Initialize descriptor sets
	cloudComputeSet = VulkanInitializers::CreateDescriptorSet(logicalDevice, descriptorPool, cloudComputeSetLayout);
	graphicsSet = VulkanInitializers::CreateDescriptorSet(logicalDevice, descriptorPool, graphicsSetLayout);

	pingPongCloudResultSet1 = VulkanInitializers::CreateDescriptorSet(logicalDevice, descriptorPool, pingPongCloudResultSetLayout);
	pingPongCloudResultSet2 = VulkanInitializers::CreateDescriptorSet(logicalDevice, descriptorPool, pingPongCloudResultSetLayout);

	cameraSet = VulkanInitializers::CreateDescriptorSet(logicalDevice, descriptorPool, cameraSetLayout);
	cameraOldSet = VulkanInitializers::CreateDescriptorSet(logicalDevice, descriptorPool, cameraSetLayout);
	timeSet = VulkanInitializers::CreateDescriptorSet(logicalDevice, descriptorPool, timeSetLayout);
	sunAndSkySet = VulkanInitializers::CreateDescriptorSet(logicalDevice, descriptorPool, sunAndSkySetLayout);
	keyPressQuerySet = VulkanInitializers::CreateDescriptorSet(logicalDevice, descriptorPool, keyPressQuerySetLayout);

	godRaysSet = VulkanInitializers::CreateDescriptorSet(logicalDevice, descriptorPool, godRaysSetLayout);
	toneMapSet1 = VulkanInitializers::CreateDescriptorSet(logicalDevice, descriptorPool, toneMapSetLayout);
	toneMapSet2 = VulkanInitializers::CreateDescriptorSet(logicalDevice, descriptorPool, toneMapSetLayout);

	TXAASet1 = VulkanInitializers::CreateDescriptorSet(logicalDevice, descriptorPool, TXAASetLayout);
	TXAASet2 = VulkanInitializers::CreateDescriptorSet(logicalDevice, descriptorPool, TXAASetLayout);

	//Create other things in the Scene like terrain models
	scene->CreateModelsInScene(graphicsCommandPool);

	//Write to and Update DescriptorSets
	WriteToAndUpdateAllDescriptorSets();
}

void Renderer::WriteToAndUpdateAllDescriptorSets()
{
	WriteToAndUpdatePingPongDescriptorSets();
	WriteToAndUpdateComputeDescriptorSets();
	WriteToAndUpdateGraphicsDescriptorSets();
	WriteToAndUpdateRemainingDescriptorSets();
	
	//Post Process Sets
	WriteToAndUpdateGodRaysSet();
	WriteToAndUpdateToneMapSet();
	WriteToAndUpdateTXAASet();
}
void Renderer::WriteToAndUpdatePingPongDescriptorSets()
{
	// Texture Compute Shader Writes To
	VkDescriptorImageInfo currentFrameTextureInfo = {};
	currentFrameTextureInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	currentFrameTextureInfo.imageView = currentCloudsResultTexture->GetTextureImageView();
	currentFrameTextureInfo.sampler = currentCloudsResultTexture->GetTextureSampler();

	// previous Frame Data for Reprojection
	VkDescriptorImageInfo previousFrameTextureInfo = {};
	previousFrameTextureInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	previousFrameTextureInfo.imageView = previousCloudsResultTexture->GetTextureImageView();
	previousFrameTextureInfo.sampler = previousCloudsResultTexture->GetTextureSampler();

	std::array<VkWriteDescriptorSet, 2> writePingPongSet1Info = {};
	
	writePingPongSet1Info[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writePingPongSet1Info[0].pNext = NULL;
	writePingPongSet1Info[0].dstSet = pingPongCloudResultSet1;
	writePingPongSet1Info[0].dstBinding = 0;
	writePingPongSet1Info[0].descriptorCount = 1;
	writePingPongSet1Info[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writePingPongSet1Info[0].pImageInfo = &currentFrameTextureInfo;

	writePingPongSet1Info[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writePingPongSet1Info[1].pNext = NULL;
	writePingPongSet1Info[1].dstSet = pingPongCloudResultSet1;
	writePingPongSet1Info[1].dstBinding = 1;
	writePingPongSet1Info[1].descriptorCount = 1;
	writePingPongSet1Info[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writePingPongSet1Info[1].pImageInfo = &previousFrameTextureInfo;

	std::array<VkWriteDescriptorSet, 2> writePingPongSet2Info = {};

	writePingPongSet2Info[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writePingPongSet2Info[0].pNext = NULL;
	writePingPongSet2Info[0].dstSet = pingPongCloudResultSet2;
	writePingPongSet2Info[0].dstBinding = 0;
	writePingPongSet2Info[0].descriptorCount = 1;
	writePingPongSet2Info[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writePingPongSet2Info[0].pImageInfo = &previousFrameTextureInfo;
	
	writePingPongSet2Info[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writePingPongSet2Info[1].pNext = NULL;
	writePingPongSet2Info[1].dstSet = pingPongCloudResultSet2;
	writePingPongSet2Info[1].dstBinding = 1;
	writePingPongSet2Info[1].descriptorCount = 1;
	writePingPongSet2Info[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writePingPongSet2Info[1].pImageInfo = &currentFrameTextureInfo;

	vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(writePingPongSet1Info.size()), writePingPongSet1Info.data(), 0, nullptr);
	vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(writePingPongSet2Info.size()), writePingPongSet2Info.data(), 0, nullptr);
}
void Renderer::WriteToAndUpdateComputeDescriptorSets()
{
	//------------------------------------------
	//-------Compute Pipeline DescriptorSets----
	//------------------------------------------
	// Cloud Low Frequency Noise
	VkDescriptorImageInfo cloudLowFrequencyNoiseImageInfo = {};
	cloudLowFrequencyNoiseImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	cloudLowFrequencyNoiseImageInfo.imageView = sky->cloudBaseShapeTexture->GetTextureImageView();
	cloudLowFrequencyNoiseImageInfo.sampler = sky->cloudBaseShapeTexture->GetTextureSampler();

	// Cloud High Frequency Noise
	VkDescriptorImageInfo cloudHighFrequencyNoiseImageInfo = {};
	cloudHighFrequencyNoiseImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	cloudHighFrequencyNoiseImageInfo.imageView = sky->cloudDetailsTexture->GetTextureImageView();
	cloudHighFrequencyNoiseImageInfo.sampler = sky->cloudDetailsTexture->GetTextureSampler();

	// Cloud Curl Noise
	VkDescriptorImageInfo cloudCurlNoiseImageInfo = {};
	cloudCurlNoiseImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	cloudCurlNoiseImageInfo.imageView = sky->cloudMotionTexture->GetTextureImageView();
	cloudCurlNoiseImageInfo.sampler = sky->cloudMotionTexture->GetTextureSampler();

	// Weather Map
	VkDescriptorImageInfo weatherMapInfo = {};
	weatherMapInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	weatherMapInfo.imageView = sky->weatherMapTexture->GetTextureImageView();
	weatherMapInfo.sampler = sky->weatherMapTexture->GetTextureSampler();

	// God Rays Creation Data Texture
	VkDescriptorImageInfo godRaysCreationDataTextureInfo = {};
	godRaysCreationDataTextureInfo.imageLayout = godRaysCreationDataTexture->GetTextureLayout();
	godRaysCreationDataTextureInfo.imageView = godRaysCreationDataTexture->GetTextureImageView();
	godRaysCreationDataTextureInfo.sampler = godRaysCreationDataTexture->GetTextureSampler();

	std::array<VkWriteDescriptorSet, 5> writeComputeTextureInfo = {};
	
	writeComputeTextureInfo[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeComputeTextureInfo[0].pNext = NULL;
	writeComputeTextureInfo[0].dstSet = cloudComputeSet;
	writeComputeTextureInfo[0].dstBinding = 0;
	writeComputeTextureInfo[0].descriptorCount = 1;
	writeComputeTextureInfo[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeComputeTextureInfo[0].pImageInfo = &cloudLowFrequencyNoiseImageInfo;

	writeComputeTextureInfo[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeComputeTextureInfo[1].pNext = NULL;
	writeComputeTextureInfo[1].dstSet = cloudComputeSet;
	writeComputeTextureInfo[1].dstBinding = 1;
	writeComputeTextureInfo[1].descriptorCount = 1;
	writeComputeTextureInfo[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeComputeTextureInfo[1].pImageInfo = &cloudHighFrequencyNoiseImageInfo;

	writeComputeTextureInfo[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeComputeTextureInfo[2].pNext = NULL;
	writeComputeTextureInfo[2].dstSet = cloudComputeSet;
	writeComputeTextureInfo[2].dstBinding = 2;
	writeComputeTextureInfo[2].descriptorCount = 1;
	writeComputeTextureInfo[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeComputeTextureInfo[2].pImageInfo = &cloudCurlNoiseImageInfo;

	writeComputeTextureInfo[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeComputeTextureInfo[3].pNext = NULL;
	writeComputeTextureInfo[3].dstSet = cloudComputeSet;
	writeComputeTextureInfo[3].dstBinding = 3;
	writeComputeTextureInfo[3].descriptorCount = 1;
	writeComputeTextureInfo[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeComputeTextureInfo[3].pImageInfo = &weatherMapInfo;

	writeComputeTextureInfo[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeComputeTextureInfo[4].pNext = NULL;
	writeComputeTextureInfo[4].dstSet = cloudComputeSet;
	writeComputeTextureInfo[4].dstBinding = 4;
	writeComputeTextureInfo[4].descriptorCount = 1;
	writeComputeTextureInfo[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writeComputeTextureInfo[4].pImageInfo = &godRaysCreationDataTextureInfo;

	vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(writeComputeTextureInfo.size()), writeComputeTextureInfo.data(), 0, nullptr);
}
void Renderer::WriteToAndUpdateGraphicsDescriptorSets()
{
	//---------------------------------
	//---- Graphics DescriptorSets ----
	//---------------------------------
	// We know we are only drawing one model but change this if creating multiple models
	std::vector<Model*> models = scene->GetModels();

	// Model
	VkDescriptorBufferInfo modelBufferInfo = {};
	modelBufferInfo.buffer = models[0]->GetModelBuffer();
	modelBufferInfo.offset = 0;
	modelBufferInfo.range = sizeof(ModelBufferObject);

	// Texture
	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = models[0]->GetTextureView();
	imageInfo.sampler = models[0]->GetTextureSampler();

	std::array<VkWriteDescriptorSet, 2> writeGraphicsSetInfo = {};

	writeGraphicsSetInfo[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeGraphicsSetInfo[0].dstSet = graphicsSet;
	writeGraphicsSetInfo[0].dstBinding = 0;
	writeGraphicsSetInfo[0].descriptorCount = 1;
	writeGraphicsSetInfo[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeGraphicsSetInfo[0].pBufferInfo = &modelBufferInfo;

	writeGraphicsSetInfo[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeGraphicsSetInfo[1].pNext = NULL;
	writeGraphicsSetInfo[1].dstSet = graphicsSet;
	writeGraphicsSetInfo[1].dstBinding = 1;
	writeGraphicsSetInfo[1].descriptorCount = 1;
	writeGraphicsSetInfo[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeGraphicsSetInfo[1].pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(writeGraphicsSetInfo.size()), writeGraphicsSetInfo.data(), 0, nullptr);
}
void Renderer::WriteToAndUpdateRemainingDescriptorSets()
{
	//--------------------------------------------------------------------
	//---- Descriptor Sets that can be attached to multiple pipelines ----
	//--------------------------------------------------------------------

	// Camera Descriptor
	VkDescriptorBufferInfo cameraBufferInfo = {};
	cameraBufferInfo.buffer = camera->GetBuffer();
	cameraBufferInfo.offset = 0;
	cameraBufferInfo.range = sizeof(CameraUBO);

	std::array<VkWriteDescriptorSet, 1> writeCameraSetInfo = {};
	writeCameraSetInfo[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeCameraSetInfo[0].dstSet = cameraSet;
	writeCameraSetInfo[0].dstBinding = 0;
	writeCameraSetInfo[0].descriptorCount = 1;									// How many 
	writeCameraSetInfo[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeCameraSetInfo[0].pBufferInfo = &cameraBufferInfo;

	vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(writeCameraSetInfo.size()), writeCameraSetInfo.data(), 0, nullptr);

	VkDescriptorBufferInfo cameraOldBufferInfo = {};
	cameraOldBufferInfo.buffer = cameraOld->GetBuffer();
	cameraOldBufferInfo.offset = 0;
	cameraOldBufferInfo.range = sizeof(CameraUBO);

	std::array<VkWriteDescriptorSet, 1> writeCameraOldSetInfo = {};
	writeCameraOldSetInfo[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeCameraOldSetInfo[0].dstSet = cameraOldSet;
	writeCameraOldSetInfo[0].dstBinding = 0;
	writeCameraOldSetInfo[0].descriptorCount = 1;									// How many 
	writeCameraOldSetInfo[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeCameraOldSetInfo[0].pBufferInfo = &cameraOldBufferInfo;

	vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(writeCameraOldSetInfo.size()), writeCameraOldSetInfo.data(), 0, nullptr);

	// Time Descriptor
	VkDescriptorBufferInfo timeBufferInfo = {};
	timeBufferInfo.buffer = scene->GetTimeBuffer();
	timeBufferInfo.offset = 0;
	timeBufferInfo.range = sizeof(Time);

	std::array<VkWriteDescriptorSet, 1> writeTimeSetInfo = {};
	writeTimeSetInfo[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeTimeSetInfo[0].pNext = NULL;
	writeTimeSetInfo[0].dstSet = timeSet;
	writeTimeSetInfo[0].dstBinding = 0;
	writeTimeSetInfo[0].descriptorCount = 1;
	writeTimeSetInfo[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeTimeSetInfo[0].pBufferInfo = &timeBufferInfo;

	vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(writeTimeSetInfo.size()), writeTimeSetInfo.data(), 0, nullptr);

	// SunAndSky Descriptor
	VkDescriptorBufferInfo sunAndSkyBufferInfo = {};
	sunAndSkyBufferInfo.buffer = sky->GetSunAndSkyBuffer();
	sunAndSkyBufferInfo.offset = 0;
	sunAndSkyBufferInfo.range = sizeof(SunAndSky);

	std::array<VkWriteDescriptorSet, 1> writeSunAndSkySetInfo = {};
	writeSunAndSkySetInfo[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSunAndSkySetInfo[0].pNext = NULL;
	writeSunAndSkySetInfo[0].dstSet = sunAndSkySet;
	writeSunAndSkySetInfo[0].dstBinding = 0;
	writeSunAndSkySetInfo[0].descriptorCount = 1;
	writeSunAndSkySetInfo[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeSunAndSkySetInfo[0].pBufferInfo = &sunAndSkyBufferInfo;

	vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(writeSunAndSkySetInfo.size()), writeSunAndSkySetInfo.data(), 0, nullptr);

	// KeyPressQuery Descriptor
	VkDescriptorBufferInfo keyPressQueryBufferInfo = {};
	keyPressQueryBufferInfo.buffer = scene->GetKeyPressQueryBuffer();
	keyPressQueryBufferInfo.offset = 0;
	keyPressQueryBufferInfo.range = sizeof(KeyPressQuery);

	std::array<VkWriteDescriptorSet, 1> writeKeyPressQuerySetInfo = {};
	writeKeyPressQuerySetInfo[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeKeyPressQuerySetInfo[0].pNext = NULL;
	writeKeyPressQuerySetInfo[0].dstSet = keyPressQuerySet;
	writeKeyPressQuerySetInfo[0].dstBinding = 0;
	writeKeyPressQuerySetInfo[0].descriptorCount = 1;
	writeKeyPressQuerySetInfo[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeKeyPressQuerySetInfo[0].pBufferInfo = &keyPressQueryBufferInfo;

	vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(writeKeyPressQuerySetInfo.size()), writeKeyPressQuerySetInfo.data(), 0, nullptr);
}

void Renderer::WriteToAndUpdateGodRaysSet()
{
	VkDescriptorImageInfo godRaysDataTextureInfo = {};
	godRaysDataTextureInfo.imageLayout = godRaysCreationDataTexture->GetTextureLayout();
	godRaysDataTextureInfo.imageView = godRaysCreationDataTexture->GetTextureImageView();
	godRaysDataTextureInfo.sampler = godRaysCreationDataTexture->GetTextureSampler();

	std::array<VkWriteDescriptorSet, 1> writeGodRaysPassInfo = {};
	writeGodRaysPassInfo[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeGodRaysPassInfo[0].pNext = NULL;
	writeGodRaysPassInfo[0].dstSet = godRaysSet;
	writeGodRaysPassInfo[0].dstBinding = 0;
	writeGodRaysPassInfo[0].descriptorCount = 1;
	writeGodRaysPassInfo[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeGodRaysPassInfo[0].pImageInfo = &godRaysDataTextureInfo;

	vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(writeGodRaysPassInfo.size()), writeGodRaysPassInfo.data(), 0, nullptr);
}
void Renderer::WriteToAndUpdateToneMapSet()
{
	VkDescriptorImageInfo toneMapPassImage1Info = {};
	toneMapPassImage1Info.imageLayout = currentCloudsResultTexture->GetTextureLayout();
	toneMapPassImage1Info.imageView = currentCloudsResultTexture->GetTextureImageView();
	toneMapPassImage1Info.sampler = currentCloudsResultTexture->GetTextureSampler();

	VkDescriptorImageInfo currentFrameImageInfo = {};
	currentFrameImageInfo.imageLayout = currentFrameTexture->GetTextureLayout();
	currentFrameImageInfo.imageView = currentFrameTexture->GetTextureImageView();
	currentFrameImageInfo.sampler = currentFrameTexture->GetTextureSampler();

	VkDescriptorImageInfo toneMapPassImage2Info = {};
	toneMapPassImage2Info.imageLayout = previousCloudsResultTexture->GetTextureLayout(); 
	toneMapPassImage2Info.imageView = previousCloudsResultTexture->GetTextureImageView();
	toneMapPassImage2Info.sampler = previousCloudsResultTexture->GetTextureSampler();

	VkDescriptorImageInfo previousFrameImageInfo = {};
	previousFrameImageInfo.imageLayout = previousFrameTexture->GetTextureLayout();
	previousFrameImageInfo.imageView = previousFrameTexture->GetTextureImageView();
	previousFrameImageInfo.sampler = previousFrameTexture->GetTextureSampler();

	std::array<VkWriteDescriptorSet, 2> writeToneMapPass1Info = {};

	writeToneMapPass1Info[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeToneMapPass1Info[0].pNext = NULL;
	writeToneMapPass1Info[0].dstSet = toneMapSet1;
	writeToneMapPass1Info[0].dstBinding = 0;
	writeToneMapPass1Info[0].descriptorCount = 1;
	writeToneMapPass1Info[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeToneMapPass1Info[0].pImageInfo = &toneMapPassImage1Info;

	writeToneMapPass1Info[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeToneMapPass1Info[1].pNext = NULL;
	writeToneMapPass1Info[1].dstSet = toneMapSet1;
	writeToneMapPass1Info[1].dstBinding = 1;
	writeToneMapPass1Info[1].descriptorCount = 1;
	writeToneMapPass1Info[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writeToneMapPass1Info[1].pImageInfo = &currentFrameImageInfo;

	std::array<VkWriteDescriptorSet, 2> writeToneMapPass2Info = {};

	writeToneMapPass2Info[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeToneMapPass2Info[0].pNext = NULL;
	writeToneMapPass2Info[0].dstSet = toneMapSet2;
	writeToneMapPass2Info[0].dstBinding = 0;
	writeToneMapPass2Info[0].descriptorCount = 1;
	writeToneMapPass2Info[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeToneMapPass2Info[0].pImageInfo = &toneMapPassImage2Info;

	writeToneMapPass2Info[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeToneMapPass2Info[1].pNext = NULL;
	writeToneMapPass2Info[1].dstSet = toneMapSet2;
	writeToneMapPass2Info[1].dstBinding = 1;
	writeToneMapPass2Info[1].descriptorCount = 1;
	writeToneMapPass2Info[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writeToneMapPass2Info[1].pImageInfo = &previousFrameImageInfo;

	vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(writeToneMapPass1Info.size()), writeToneMapPass1Info.data(), 0, nullptr);
	vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(writeToneMapPass2Info.size()), writeToneMapPass2Info.data(), 0, nullptr);
}
void Renderer::WriteToAndUpdateTXAASet()
{
	VkDescriptorImageInfo currentFrameImageInfo = {};
	currentFrameImageInfo.imageLayout = currentFrameTexture->GetTextureLayout();
	currentFrameImageInfo.imageView = currentFrameTexture->GetTextureImageView();
	currentFrameImageInfo.sampler = currentFrameTexture->GetTextureSampler();

	VkDescriptorImageInfo previousFrameImageInfo = {};
	previousFrameImageInfo.imageLayout = previousFrameTexture->GetTextureLayout();
	previousFrameImageInfo.imageView = previousFrameTexture->GetTextureImageView();
	previousFrameImageInfo.sampler = previousFrameTexture->GetTextureSampler();

	std::array<VkWriteDescriptorSet, 2> writeTXAAPass1Info = {};

	writeTXAAPass1Info[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeTXAAPass1Info[0].pNext = NULL;
	writeTXAAPass1Info[0].dstSet = TXAASet1;
	writeTXAAPass1Info[0].dstBinding = 0;
	writeTXAAPass1Info[0].descriptorCount = 1;
	writeTXAAPass1Info[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeTXAAPass1Info[0].pImageInfo = &previousFrameImageInfo;

	writeTXAAPass1Info[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeTXAAPass1Info[1].pNext = NULL;
	writeTXAAPass1Info[1].dstSet = TXAASet1;
	writeTXAAPass1Info[1].dstBinding = 1;
	writeTXAAPass1Info[1].descriptorCount = 1;
	writeTXAAPass1Info[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writeTXAAPass1Info[1].pImageInfo = &currentFrameImageInfo;

	std::array<VkWriteDescriptorSet, 2> writeTXAAPass2Info = {};

	writeTXAAPass2Info[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeTXAAPass2Info[0].pNext = NULL;
	writeTXAAPass2Info[0].dstSet = TXAASet2;
	writeTXAAPass2Info[0].dstBinding = 0;
	writeTXAAPass2Info[0].descriptorCount = 1;
	writeTXAAPass2Info[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeTXAAPass2Info[0].pImageInfo = &currentFrameImageInfo;

	writeTXAAPass2Info[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeTXAAPass2Info[1].pNext = NULL;
	writeTXAAPass2Info[1].dstSet = TXAASet2;
	writeTXAAPass2Info[1].dstBinding = 1;
	writeTXAAPass2Info[1].descriptorCount = 1;
	writeTXAAPass2Info[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writeTXAAPass2Info[1].pImageInfo = &previousFrameImageInfo;

	vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(writeTXAAPass1Info.size()), writeTXAAPass1Info.data(), 0, nullptr);
	vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(writeTXAAPass2Info.size()), writeTXAAPass2Info.data(), 0, nullptr);
}

//--------------------------------------------------------
//------------- Resources Creations and Recreation -------
//--------------------------------------------------------
void Renderer::CreateResources()
{
	//To store the results of the compute shader that will be passed on to the frag shader
	currentCloudsResultTexture = new Texture2D(device, window_width, window_height, VK_FORMAT_R32G32B32A32_SFLOAT);
	currentCloudsResultTexture->createEmptyTexture(logicalDevice, physicalDevice, computeCommandPool);

	//Stores the results of the previous Frame
	previousCloudsResultTexture = new Texture2D(device, window_width, window_height, VK_FORMAT_R32G32B32A32_SFLOAT);
	previousCloudsResultTexture->createEmptyTexture(logicalDevice, physicalDevice, computeCommandPool);

	//To store the results of the compute shader that will be passed on to the frag shader
	godRaysCreationDataTexture = new Texture2D(device, window_width, window_height, VK_FORMAT_R8G8B8A8_SNORM);
	godRaysCreationDataTexture->createEmptyTexture(logicalDevice, physicalDevice, computeCommandPool);

	currentFrameTexture = new Texture2D(device, window_width, window_height, VK_FORMAT_R8G8B8A8_SNORM);
	currentFrameTexture->createEmptyTexture(logicalDevice, physicalDevice, computeCommandPool);

	previousFrameTexture = new Texture2D(device, window_width, window_height, VK_FORMAT_R8G8B8A8_SNORM);
	previousFrameTexture->createEmptyTexture(logicalDevice, physicalDevice, computeCommandPool);
}