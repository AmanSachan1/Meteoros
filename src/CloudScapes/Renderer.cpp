#include "Renderer.h"

Renderer::Renderer(VulkanDevice* device, VkPhysicalDevice physicalDevice, VulkanSwapChain* swapChain, Scene* scene, Camera* camera, uint32_t width, uint32_t height)
	: device(device), 
	logicalDevice(device->GetVkDevice()),
	physicalDevice(physicalDevice),
	swapChain(swapChain),
	scene(scene),
	camera(camera),
	window_width(width),
	window_height(height)
{
	InitializeRenderer();
}

Renderer::~Renderer() 
{
	vkDeviceWaitIdle(logicalDevice);

	// TODO: Destroy any resources you created
	DestroyResourcesDependentOnWindowSize();
	DestroyResourcesIndependentOfWindowSize();
}

void Renderer::DestroyResourcesDependentOnWindowSize()
{
	vkFreeCommandBuffers(logicalDevice, graphicsCommandPool, static_cast<uint32_t>(graphicsCommandBuffer.size()), graphicsCommandBuffer.data());
	vkFreeCommandBuffers(logicalDevice, computeCommandPool, 1, &computeCommandBuffer);

	DestroyFrameResources();

	//Regular Pipelines
	// All 3 pipelines have things that depend on the window width and height and so we need to recreate all of those resources when resizing
	//This Function recreates the frame resources which in turn means we need to recreate the graphics pipeline and rerecord the graphics command buffers
	vkDestroyPipelineLayout(logicalDevice, graphicsPipelineLayout, nullptr);
	vkDestroyPipelineLayout(logicalDevice, cloudsPipelineLayout, nullptr);
	vkDestroyPipelineLayout(logicalDevice, computePipelineLayout, nullptr);
	vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);
	vkDestroyPipeline(logicalDevice, cloudsPipeline, nullptr);
	vkDestroyPipeline(logicalDevice, computePipeline, nullptr);

	//Post Process Pipelines
	vkDestroyPipelineCache(logicalDevice, postProcessPipeLineCache, nullptr);
	vkDestroyPipelineLayout(logicalDevice, postProcess_GodRays_PipelineLayout, nullptr);
	vkDestroyPipeline(logicalDevice, postProcess_GodRays_PipeLine, nullptr);
	vkDestroyPipelineLayout(logicalDevice, postProcess_FinalPass_PipelineLayout, nullptr);
	vkDestroyPipeline(logicalDevice, postProcess_FinalPass_PipeLine, nullptr);

	//Render Pass
	vkDestroyRenderPass(logicalDevice, renderPass, nullptr);

	//Descriptor Set Layouts
	vkDestroyDescriptorSetLayout(logicalDevice, computeSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, cloudSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, graphicsSetLayout, nullptr);

	vkDestroyDescriptorSetLayout(logicalDevice, cameraSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, timeSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, sunAndSkySetLayout, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, keyPressQuerySetLayout, nullptr);

	vkDestroyDescriptorSetLayout(logicalDevice, godRaysSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, finalPassSetLayout, nullptr);
	
	//delete background quad
	delete quad;

	//Descriptor Set
	vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);

	delete scene->GetModels()[0];

	//Textures
	delete currentFrameResultTexture;
	delete previousFrameComputeResultTexture;
	delete godRaysCreationDataTexture;
}
void Renderer::DestroyResourcesIndependentOfWindowSize()
{
	//Command Pools
	vkDestroyCommandPool(logicalDevice, graphicsCommandPool, nullptr);
	vkDestroyCommandPool(logicalDevice, computeCommandPool, nullptr);

	//Descriptor Set Layouts
	vkDestroyDescriptorSetLayout(logicalDevice, timeSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, sunAndSkySetLayout, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, keyPressQuerySetLayout, nullptr);
	
	//delete background quad
	delete quad;

	//Textures
	delete cloudBaseShapeTexture;
	delete cloudDetailsTexture;
	delete cloudMotionTexture;
	delete weatherMapTexture;
}

void Renderer::InitializeRenderer()
{
	CreateCommandPools();
	CreateRenderPass();

	CreateComputeResources();
	CreatePostProcessResources();

	CreateDescriptorPool();
	CreateAllDescriptorSetLayouts();
	CreateAllDescriptorSets();

	CreateFrameResources();

	CreateAllPipeLines(renderPass, 0);

	RecordGraphicsCommandBuffer();
	RecordComputeCommandBuffer();
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
	computeSubmitInfo.pCommandBuffers = &computeCommandBuffer;

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
	depthAttachment.format = FindDepthFormat(); //The format should be the same as the depth image itself.
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

void Renderer::CreateAllPipeLines(VkRenderPass renderPass, unsigned int subpass)
{
	computePipelineLayout = CreatePipelineLayout({ computeSetLayout, cameraSetLayout });// , timeSetLayout, sunAndSkySetLayout, keyPressQuerySetLayout });
	CreateComputePipeline();

	cloudsPipelineLayout = CreatePipelineLayout({ cloudSetLayout });
	CreateCloudsPipeline(renderPass, 0);

	graphicsPipelineLayout = CreatePipelineLayout({ graphicsSetLayout, cameraSetLayout });
	CreateGraphicsPipeline(renderPass, 0);

	postProcess_GodRays_PipelineLayout = CreatePipelineLayout({ godRaysSetLayout, sunAndSkySetLayout });
	postProcess_FinalPass_PipelineLayout = CreatePipelineLayout({ finalPassSetLayout });
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

void Renderer::CreateCloudsPipeline(VkRenderPass renderPass, unsigned int subpass)
{
	//--------------------------------------------------------
	//---------------- Set up shader stages ------------------
	//--------------------------------------------------------
	// Can add more shader modules to the list of shader stages that are part of the overall graphics pipeline
	// Reference: https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Shader_modules
	// Create vert and frag shader modules
	VkShaderModule vertShaderModule = ShaderModule::createShaderModule("CloudScapes/shaders/cloudDisplay.vert.spv", device->GetVkDevice());
	VkShaderModule fragShaderModule = ShaderModule::createShaderModule("CloudScapes/shaders/cloudDisplay.frag.spv", device->GetVkDevice());

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
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL; //make fragments instead of lines or points
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
	// TODO:: Could be useful for transparency for clouds infront of Terrain or other models
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

	// -------- Create clouds pipeline ---------
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
	pipelineInfo.layout = cloudsPipelineLayout; // passed in
	pipelineInfo.renderPass = renderPass; // passed in
	pipelineInfo.subpass = subpass; // passed in
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Vulkan allows you to create a new graphics pipeline by deriving from an existing pipeline. We aren't doing this.
	pipelineInfo.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &cloudsPipeline) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create pipeline");
	}

	// No need for the shader modules anymore, so we destory them!
	vkDestroyShaderModule(logicalDevice, vertShaderModule, nullptr);
	vkDestroyShaderModule(logicalDevice, fragShaderModule, nullptr);
}

void Renderer::CreateComputePipeline()
{
	VkShaderModule compShaderModule = ShaderModule::createShaderModule("CloudScapes/shaders/cloudCompute.comp.spv", device->GetVkDevice());

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

void Renderer::CreatePostProcessPipeLines(VkRenderPass renderPass)
{
	// -------- Vertex input binding --------
	VkPipelineVertexInputStateCreateInfo emptyVertexInputState = VulkanInitializers::pipelineVertexInputStateCreateInfo();

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = 
		VulkanInitializers::pipelineInputAssemblyStateCreateInfo( VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);

	VkPipelineRasterizationStateCreateInfo rasterizationState =
		VulkanInitializers::pipelineRasterizationStateCreateInfo( VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);

	VkPipelineColorBlendAttachmentState blendAttachmentState =
		VulkanInitializers::pipelineColorBlendAttachmentState( 0xf, VK_FALSE);

	VkPipelineColorBlendStateCreateInfo colorBlendState =
		VulkanInitializers::pipelineColorBlendStateCreateInfo( 1, &blendAttachmentState);

	VkPipelineDepthStencilStateCreateInfo depthStencilState =
		VulkanInitializers::pipelineDepthStencilStateCreateInfo( VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);

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
		VulkanInitializers::pipelineMultisampleStateCreateInfo( VK_SAMPLE_COUNT_1_BIT, 0);

	// No Dynamic states
	//std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	//VkPipelineDynamicStateCreateInfo dynamicState =
	//	VulkanInitializers::pipelineDynamicStateCreateInfo( dynamicStateEnables, 0 );

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages; //Defined later on and changes per pipeline binding since we are creating many pipelines here

	// -------- Create Base PostProcess pipeline Info ---------
	VkGraphicsPipelineCreateInfo postProcessPipelineCreateInfo =
		VulkanInitializers::graphicsPipelineCreateInfo( postProcess_GodRays_PipelineLayout, renderPass, 0);
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

	// -------- God Rays pipeline -----------------------------------------
	VkShaderModule godRays_vertShaderModule = ShaderModule::createShaderModule("CloudScapes/shaders/postProcess_GodRays.vert.spv", logicalDevice);
	VkShaderModule godRays_fragShaderModule = ShaderModule::createShaderModule("CloudScapes/shaders/postProcess_GodRays.frag.spv", logicalDevice);

	// Assign each shader module to the appropriate stage in the pipeline
	shaderStages[0] = VulkanInitializers::loadShader(VK_SHADER_STAGE_VERTEX_BIT, godRays_vertShaderModule);
	shaderStages[1] = VulkanInitializers::loadShader(VK_SHADER_STAGE_FRAGMENT_BIT, godRays_fragShaderModule);

	// Empty vertex input state

	postProcessPipelineCreateInfo.layout = postProcess_GodRays_PipelineLayout;

	if (vkCreateGraphicsPipelines(logicalDevice, postProcessPipeLineCache, 1, &postProcessPipelineCreateInfo, nullptr, &postProcess_GodRays_PipeLine) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create post process pipeline");
	}

	vkDestroyShaderModule(device->GetVkDevice(), godRays_vertShaderModule, nullptr);
	vkDestroyShaderModule(device->GetVkDevice(), godRays_fragShaderModule, nullptr);

	// -------- Anti Aliasing  pipeline -----------------------------------------

	// -------- Final Pass pipeline -----------------------------------------
	// Does Compositing and Tone Mapping

	VkShaderModule finalPass_vertShaderModule = ShaderModule::createShaderModule("CloudScapes/shaders/postProcess_FinalPass.vert.spv", logicalDevice);
	VkShaderModule finalPass_fragShaderModule = ShaderModule::createShaderModule("CloudScapes/shaders/postProcess_FinalPass.frag.spv", logicalDevice);

	// Assign each shader module to the appropriate stage in the pipeline
	shaderStages[0] = VulkanInitializers::loadShader(VK_SHADER_STAGE_VERTEX_BIT, finalPass_vertShaderModule);
	shaderStages[1] = VulkanInitializers::loadShader(VK_SHADER_STAGE_FRAGMENT_BIT, finalPass_fragShaderModule);

	// Empty vertex input state
	postProcessPipelineCreateInfo.layout = postProcess_FinalPass_PipelineLayout;

	if (vkCreateGraphicsPipelines(logicalDevice, postProcessPipeLineCache, 1, &postProcessPipelineCreateInfo, nullptr, &postProcess_FinalPass_PipeLine) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create post process pipeline");
	}

	vkDestroyShaderModule(device->GetVkDevice(), finalPass_vertShaderModule, nullptr);
	vkDestroyShaderModule(device->GetVkDevice(), finalPass_fragShaderModule, nullptr);
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
	DestroyResourcesDependentOnWindowSize();

	//To store the results of the compute shader that will be passed on to the frag shader
	currentFrameResultTexture = new Texture2D(device, window_width, window_height, VK_FORMAT_R8G8B8A8_UNORM);
	currentFrameResultTexture->createEmptyTexture(logicalDevice, physicalDevice, computeCommandPool);
	//Stores the results of the previous Frame
	previousFrameComputeResultTexture = new Texture2D(device, window_width, window_height, VK_FORMAT_R8G8B8A8_UNORM);
	previousFrameComputeResultTexture->createEmptyTexture(logicalDevice, physicalDevice, computeCommandPool);
	
	CreatePostProcessResources();
	
	CreateRenderPass();

	CreateDescriptorPool();
	CreateAllDescriptorSetLayouts();
	CreateAllDescriptorSets();

	CreateFrameResources();
	CreateAllPipeLines(renderPass, 0);

	RecordGraphicsCommandBuffer();
	RecordComputeCommandBuffer();
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
	VkFormat depthFormat = FindDepthFormat();
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

	if (vkAllocateCommandBuffers(logicalDevice, &commandBufferAllocateInfo, graphicsCommandBuffer.data()) != VK_SUCCESS) {
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
		if (vkBeginCommandBuffer(graphicsCommandBuffer[i], &beginInfo) != VK_SUCCESS) {
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
		imageMemoryBarrier.image = currentFrameResultTexture->GetTextureImage();
		imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;		

		// A pipeline barrier inserts an execution dependency and a set of memory dependencies between a set of commands earlier in the command buffer and a set of commands later in the command buffer.
		// Reference: https://vulkan.lunarg.com/doc/view/1.0.30.0/linux/vkspec.chunked/ch06s05.html
		vkCmdPipelineBarrier(graphicsCommandBuffer[i],
							 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
							 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
							 0,
							 0, nullptr,
							 0, nullptr,
							 1, &imageMemoryBarrier);

		vkCmdBeginRenderPass(graphicsCommandBuffer[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		// VK_SUBPASS_CONTENTS_INLINE: The render pass commands will be embedded in the primary command
		// buffer itself and no secondary command buffers will be executed.

		//------------------------
		//--- Clouds Pipeline---
		//------------------------

		// Bind the clouds pipeline
		vkCmdBindPipeline(graphicsCommandBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, cloudsPipeline);
		
		// Bind sampler descriptor set
		vkCmdBindDescriptorSets(graphicsCommandBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, cloudsPipelineLayout, 0, 1, &cloudSet, 0, nullptr);

		// Bind the vertex and index buffers
		VkDeviceSize quadOffsets[] = { 0 };
		const VkBuffer quadVertices = quad->getVertexBuffer();
		vkCmdBindVertexBuffers(graphicsCommandBuffer[i], 0, 1, &quadVertices, quadOffsets);

		// Bind triangle index buffer
		vkCmdBindIndexBuffer(graphicsCommandBuffer[i], quad->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

		// Draw indexed triangle
		/*
		vkCmdDrawIndexed has the following parameters, aside from the command buffer:
		indexCount;
		instanceCount: Used for instanced rendering, use 1 if you're not doing that.
		firstIndex:  Used as an offset into the index buffer
		vertexOffset: Used as an offset into the vertex buffer
		firstInstance: Used as an offset for instanced rendering, defines the lowest value of gl_InstanceIndex.
		*/
		vkCmdDrawIndexed(graphicsCommandBuffer[i], quad->getIndexBufferSize(), 1, 0, 0, 1);

		//------------------------
		//--- Graphics Pipeline---
		//------------------------

		//// Bind the graphics pipeline
		//vkCmdBindPipeline(graphicsCommandBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		//// Bind graphics descriptor set
		//vkCmdBindDescriptorSets(graphicsCommandBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout, 0, 1, &graphicsSet, 0, nullptr);
		//vkCmdBindDescriptorSets(graphicsCommandBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout, 1, 1, &cameraSet, 0, nullptr);

		//// Bind the vertex and index buffers
		//VkDeviceSize geomOffsets[] = { 0 };
		//const VkBuffer geomVertices = scene->GetModels()[0]->getVertexBuffer();
		//vkCmdBindVertexBuffers(graphicsCommandBuffer[i], 0, 1, &geomVertices, geomOffsets);

		//// Bind triangle index buffer
		//vkCmdBindIndexBuffer(graphicsCommandBuffer[i], scene->GetModels()[0]->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

		//// Draw indexed triangle
		//vkCmdDrawIndexed(graphicsCommandBuffer[i], scene->GetModels()[0]->getIndexBufferSize(), 1, 0, 0, 1);

		//-----------------------------
		//--- PostProcess Pipelines ---
		//-----------------------------
		//// God Rays Pipeline
		//vkCmdBindDescriptorSets(graphicsCommandBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, postProcess_GodRays_PipelineLayout, 0, 1, &godRaysSet, 0, NULL);
		//vkCmdBindPipeline(graphicsCommandBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, postProcess_GodRays_PipeLine);
		//vkCmdDraw(graphicsCommandBuffer[i], 3, 1, 0, 0);

		//// Final Pass Pipeline
		//vkCmdBindDescriptorSets(graphicsCommandBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, postProcess_FinalPass_PipelineLayout, 0, 1, &finalPassSet, 0, NULL);
		//vkCmdBindPipeline(graphicsCommandBuffer[i], VK_PIPELINE_BIND_POINT_GRAPHICS, postProcess_FinalPass_PipeLine);
		//vkCmdDraw(graphicsCommandBuffer[i], 3, 1, 0, 0);

		//---------- End RenderPass ---------
		vkCmdEndRenderPass(graphicsCommandBuffer[i]);

		//---------- End Recording ----------
		if (vkEndCommandBuffer(graphicsCommandBuffer[i]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to record the graphics command buffer");
		}
	}// end for loop
}

void Renderer::RecordComputeCommandBuffer()
{
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.commandPool = computeCommandPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = 1;

	if (vkAllocateCommandBuffers(logicalDevice, &commandBufferAllocateInfo, &computeCommandBuffer) != VK_SUCCESS)
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
	if (vkBeginCommandBuffer(computeCommandBuffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("Failed to begin recording compute command buffer");
	}

	//-----------------------------------------------------
	//--- Compute Pipeline Binding, Dispatch & Barriers ---
	//-----------------------------------------------------
	//Bind the compute piepline
	vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);

	//Bind Descriptor Sets for compute
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeSet, 0, nullptr);
	vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 1, 1, &cameraSet, 0, nullptr);
	
	// Dispatch the compute kernel
	// similar to a kernel call --> void vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);	
	uint32_t numBlocksX = (window_width + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
	uint32_t numBlocksY = (window_height + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
	uint32_t numBlocksZ = 1;
	vkCmdDispatch(computeCommandBuffer, numBlocksX, numBlocksY, numBlocksZ);

	//---------- End Recording ----------
	if (vkEndCommandBuffer(computeCommandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("Failed to record the compute command buffer");
	}
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
		// ------------ Compute -----------------
		// Compute Texture Write 
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
		// Previous Frame Data as Texture for Reprojection
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
		// Samplers for all the cloud Textures
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }, // low frequency texture
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }, // high frequency texture
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }, // curl noise texture
		// Weather Map
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
		// Store Texture for God Rays PostProcess
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },

		// ------------ Graphics -----------------
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }, //model matrix
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }, //texture sampler for model

		// ------------ Cloud Pipeline -----------
		// Sampler in the clouds pipeline that reads the image created by the compute Shader 
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },

		// ------------ Can be attached to multiple pipelines -----------------

		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }, // Camera
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }, // Time
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }, // SunAndSky
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }, // KeyPress

		// ------------ PostProcess pipelines -----------------
		// GodRays -- GreyScale Image of where light is in the sky
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
		// Anti Aliasing //TODO
		// Final Pass with Tone Mapping
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
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

	//-------------------- Computes Pipeline --------------------
	VkDescriptorSetLayoutBinding currentFrameResultLayoutBinding =         { 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
	VkDescriptorSetLayoutBinding previousFrameComputeResultLayoutBinding = { 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
	VkDescriptorSetLayoutBinding cloudLowFrequencyNoiseSetLayoutBinding =  { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
	VkDescriptorSetLayoutBinding cloudHighFrequencyNoiseSetLayoutBinding = { 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
	VkDescriptorSetLayoutBinding cloudCurlNoiseSetLayoutBinding =          { 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
	VkDescriptorSetLayoutBinding weatherMapSetLayoutBinding =              { 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
	VkDescriptorSetLayoutBinding godRaysCreationDataSetLayoutBinding =	   { 6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };

	std::array<VkDescriptorSetLayoutBinding, 7> computeBindings = { currentFrameResultLayoutBinding,
																	previousFrameComputeResultLayoutBinding,
																	cloudLowFrequencyNoiseSetLayoutBinding, 
																	cloudHighFrequencyNoiseSetLayoutBinding,
																	cloudCurlNoiseSetLayoutBinding,
																	weatherMapSetLayoutBinding,
																	godRaysCreationDataSetLayoutBinding };
	VkDescriptorSetLayoutCreateInfo computeLayoutInfo = {};
	computeLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	computeLayoutInfo.bindingCount = static_cast<uint32_t>(computeBindings.size());
	computeLayoutInfo.pBindings = computeBindings.data();
	if (vkCreateDescriptorSetLayout(logicalDevice, &computeLayoutInfo, nullptr, &computeSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}

	//-------------------- Graphics Pipeline --------------------
	VkDescriptorSetLayoutBinding modelSetLayoutBinding = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr };
	VkDescriptorSetLayoutBinding samplerSetLayoutBinding = { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };
	
	std::array<VkDescriptorSetLayoutBinding, 2> graphicsBindings = { modelSetLayoutBinding, samplerSetLayoutBinding };
	VkDescriptorSetLayoutCreateInfo graphicsLayoutInfo = {};
	graphicsLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	graphicsLayoutInfo.bindingCount = static_cast<uint32_t>(graphicsBindings.size());
	graphicsLayoutInfo.pBindings = graphicsBindings.data();
	if (vkCreateDescriptorSetLayout(logicalDevice, &graphicsLayoutInfo, nullptr, &graphicsSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}

	//-------------------- Clouds Pipelines --------------------
	VkDescriptorSetLayoutBinding cloudPostComputeSetLayoutBinding = { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };

	std::array<VkDescriptorSetLayoutBinding, 1> cloudBindings = { cloudPostComputeSetLayoutBinding };
	VkDescriptorSetLayoutCreateInfo cloudLayoutInfo = {};
	cloudLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	cloudLayoutInfo.bindingCount = static_cast<uint32_t>(cloudBindings.size());
	cloudLayoutInfo.pBindings = cloudBindings.data();
	if (vkCreateDescriptorSetLayout(logicalDevice, &cloudLayoutInfo, nullptr, &cloudSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}

	//-------------------- Descriptors that are discrete and mey be attached to multiple pieplines --------------------
	//Camera
	VkDescriptorSetLayoutBinding cameraSetLayoutBinding = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr };

	std::array<VkDescriptorSetLayoutBinding, 1> cameraBindings = { cameraSetLayoutBinding };
	VkDescriptorSetLayoutCreateInfo cameraLayoutInfo = {};
	cameraLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	cameraLayoutInfo.bindingCount = static_cast<uint32_t>(cameraBindings.size());
	cameraLayoutInfo.pBindings = cameraBindings.data();
	if (vkCreateDescriptorSetLayout(logicalDevice, &cameraLayoutInfo, nullptr, &cameraSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}

	//Time --> Would be better to use pushConstants --> TODO at somepoint
	VkDescriptorSetLayoutBinding timeSetLayoutBinding = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr };

	std::array<VkDescriptorSetLayoutBinding, 1> timeBindings = { timeSetLayoutBinding };
	VkDescriptorSetLayoutCreateInfo timeLayoutInfo = {};
	timeLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	timeLayoutInfo.bindingCount = static_cast<uint32_t>(timeBindings.size());
	timeLayoutInfo.pBindings = timeBindings.data();
	if (vkCreateDescriptorSetLayout(logicalDevice, &timeLayoutInfo, nullptr, &timeSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}

	//SunAndSky
	VkDescriptorSetLayoutBinding sunAndSkySetLayoutBinding = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr };

	std::array<VkDescriptorSetLayoutBinding, 1> sunAndSkyBindings = { sunAndSkySetLayoutBinding };
	VkDescriptorSetLayoutCreateInfo sunAndSkyLayoutInfo = {};
	sunAndSkyLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	sunAndSkyLayoutInfo.bindingCount = static_cast<uint32_t>(sunAndSkyBindings.size());
	sunAndSkyLayoutInfo.pBindings = sunAndSkyBindings.data();
	if (vkCreateDescriptorSetLayout(logicalDevice, &sunAndSkyLayoutInfo, nullptr, &sunAndSkySetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}

	//KeyPressQuery
	VkDescriptorSetLayoutBinding keyPressQuerySetLayoutBinding = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr };

	std::array<VkDescriptorSetLayoutBinding, 1> keyPressQueryBindings = { keyPressQuerySetLayoutBinding };
	VkDescriptorSetLayoutCreateInfo keyPressQueryLayoutInfo = {};
	keyPressQueryLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	keyPressQueryLayoutInfo.bindingCount = static_cast<uint32_t>(keyPressQueryBindings.size());
	keyPressQueryLayoutInfo.pBindings = keyPressQueryBindings.data();
	if (vkCreateDescriptorSetLayout(logicalDevice, &keyPressQueryLayoutInfo, nullptr, &keyPressQuerySetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}

	//-------------------- Post Process --------------------
	//God Rays
	VkDescriptorSetLayoutBinding resultantImageSetLayoutBinding = { 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };
	VkDescriptorSetLayoutBinding godRaysSetLayoutBinding = { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };

	std::array<VkDescriptorSetLayoutBinding, 2> godRaysBindings = { resultantImageSetLayoutBinding, godRaysSetLayoutBinding };
	VkDescriptorSetLayoutCreateInfo godRaysLayoutInfo = {};
	godRaysLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	godRaysLayoutInfo.bindingCount = static_cast<uint32_t>(godRaysBindings.size());
	godRaysLayoutInfo.pBindings = godRaysBindings.data();
	if (vkCreateDescriptorSetLayout(logicalDevice, &godRaysLayoutInfo, nullptr, &godRaysSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}

	//Final Pass
	VkDescriptorSetLayoutBinding preFinalImageSetLayoutBinding = { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };

	std::array<VkDescriptorSetLayoutBinding, 1> finalPassBindings = { preFinalImageSetLayoutBinding };
	VkDescriptorSetLayoutCreateInfo finalPassLayoutInfo = {};
	finalPassLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	finalPassLayoutInfo.bindingCount = static_cast<uint32_t>(finalPassBindings.size());
	finalPassLayoutInfo.pBindings = finalPassBindings.data();
	if (vkCreateDescriptorSetLayout(logicalDevice, &finalPassLayoutInfo, nullptr, &finalPassSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}
}

void Renderer::CreateAllDescriptorSets()
{
	// Initialize descriptor sets
	computeSet = CreateDescriptorSet(descriptorPool, computeSetLayout);
	cloudSet = CreateDescriptorSet(descriptorPool, cloudSetLayout);
	graphicsSet = CreateDescriptorSet(descriptorPool, graphicsSetLayout);

	cameraSet = CreateDescriptorSet(descriptorPool, cameraSetLayout);
	timeSet = CreateDescriptorSet(descriptorPool, timeSetLayout);
	sunAndSkySet = CreateDescriptorSet(descriptorPool, sunAndSkySetLayout);
	keyPressQuerySet = CreateDescriptorSet(descriptorPool, keyPressQuerySetLayout);

	godRaysSet = CreateDescriptorSet(descriptorPool, godRaysSetLayout);
	finalPassSet = CreateDescriptorSet(descriptorPool, finalPassSetLayout);
	
	scene->CreateModelsInScene(graphicsCommandPool);

	// Background Quad model
	const std::vector<Vertex> quadVertices = {
		{ { -1.0f, -1.0f, 0.99999f, 1.0f },{ 1.0f, 0.0f, 0.0f, 1.0f },{ 0.0f, 0.0f } },
		{ { 1.0f,  -1.0f, 0.99999f, 1.0f },{ 0.0f, 1.0f, 0.0f, 1.0f },{ 1.0f, 0.0f } },
		{ { 1.0f,  1.0f, 0.99999f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f },{ 1.0f, 1.0f } },
		{ { -1.0f, 1.0f, 0.99999f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f },{ 0.0f, 1.0f } },
	};
	std::vector<unsigned int> quadIndices = { 0, 1, 2, 2, 3, 0, };
	quad = new Model(device, graphicsCommandPool, quadVertices, quadIndices);

	//Write to and Update DescriptorSets
	WriteToAndUpdateAllDescriptorSets();
}

void Renderer::WriteToAndUpdateAllDescriptorSets()
{
	WriteToAndUpdateComputeDescriptorSets();
	WriteToAndUpdateGraphicsDescriptorSets();
	WriteToAndUpdateCloudsDescriptorSets();
	WriteToAndUpdateRemainingDescriptorSets();
	WriteToAndUpdatePostDescriptorSets();
}

void Renderer::WriteToAndUpdateComputeDescriptorSets()
{
	//------------------------------------------
	//-------Compute Pipeline DescriptorSets----
	//------------------------------------------
	// Texture Compute Shader Writes To
	VkDescriptorImageInfo currentFrameTextureInfo = {};
	currentFrameTextureInfo.imageLayout = currentFrameResultTexture->GetTextureLayout();
	currentFrameTextureInfo.imageView = currentFrameResultTexture->GetTextureImageView();
	currentFrameTextureInfo.sampler = currentFrameResultTexture->GetTextureSampler();

	// previous Frame Data for Reprojection
	VkDescriptorImageInfo previousFrameTextureInfo = {};
	previousFrameTextureInfo.imageLayout = previousFrameComputeResultTexture->GetTextureLayout();
	previousFrameTextureInfo.imageView = previousFrameComputeResultTexture->GetTextureImageView();
	previousFrameTextureInfo.sampler = previousFrameComputeResultTexture->GetTextureSampler();

	// Cloud Low Frequency Noise
	VkDescriptorImageInfo cloudLowFrequencyNoiseImageInfo = {};
	cloudLowFrequencyNoiseImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	cloudLowFrequencyNoiseImageInfo.imageView = cloudBaseShapeTexture->GetTextureImageView();
	cloudLowFrequencyNoiseImageInfo.sampler = cloudBaseShapeTexture->GetTextureSampler();

	// Cloud High Frequency Noise
	VkDescriptorImageInfo cloudHighFrequencyNoiseImageInfo = {};
	cloudHighFrequencyNoiseImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	cloudHighFrequencyNoiseImageInfo.imageView = cloudDetailsTexture->GetTextureImageView();
	cloudHighFrequencyNoiseImageInfo.sampler = cloudDetailsTexture->GetTextureSampler();

	// Cloud Curl Noise
	VkDescriptorImageInfo cloudCurlNoiseImageInfo = {};
	cloudCurlNoiseImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	cloudCurlNoiseImageInfo.imageView = cloudMotionTexture->GetTextureImageView();
	cloudCurlNoiseImageInfo.sampler = cloudMotionTexture->GetTextureSampler();

	// Weather Map
	VkDescriptorImageInfo weatherMapInfo = {};
	weatherMapInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	weatherMapInfo.imageView = weatherMapTexture->GetTextureImageView();
	weatherMapInfo.sampler = weatherMapTexture->GetTextureSampler();

	// God Rays Creation Data Texture
	VkDescriptorImageInfo godRaysCreationDataTextureInfo = {};
	godRaysCreationDataTextureInfo.imageLayout = godRaysCreationDataTexture->GetTextureLayout();
	godRaysCreationDataTextureInfo.imageView = godRaysCreationDataTexture->GetTextureImageView();
	godRaysCreationDataTextureInfo.sampler = godRaysCreationDataTexture->GetTextureSampler();

	std::array<VkWriteDescriptorSet, 7> writeComputeTextureInfo = {};
	writeComputeTextureInfo[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeComputeTextureInfo[0].pNext = NULL;
	writeComputeTextureInfo[0].dstSet = computeSet;
	writeComputeTextureInfo[0].dstBinding = 0;
	writeComputeTextureInfo[0].descriptorCount = 1;
	writeComputeTextureInfo[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writeComputeTextureInfo[0].pImageInfo = &currentFrameTextureInfo;

	writeComputeTextureInfo[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeComputeTextureInfo[1].pNext = NULL;
	writeComputeTextureInfo[1].dstSet = computeSet;
	writeComputeTextureInfo[1].dstBinding = 1;
	writeComputeTextureInfo[1].descriptorCount = 1;
	writeComputeTextureInfo[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writeComputeTextureInfo[1].pImageInfo = &previousFrameTextureInfo;

	writeComputeTextureInfo[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeComputeTextureInfo[2].pNext = NULL;
	writeComputeTextureInfo[2].dstSet = computeSet;
	writeComputeTextureInfo[2].dstBinding = 2;
	writeComputeTextureInfo[2].descriptorCount = 1;
	writeComputeTextureInfo[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeComputeTextureInfo[2].pImageInfo = &cloudLowFrequencyNoiseImageInfo;

	writeComputeTextureInfo[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeComputeTextureInfo[3].pNext = NULL;
	writeComputeTextureInfo[3].dstSet = computeSet;
	writeComputeTextureInfo[3].dstBinding = 3;
	writeComputeTextureInfo[3].descriptorCount = 1;
	writeComputeTextureInfo[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeComputeTextureInfo[3].pImageInfo = &cloudHighFrequencyNoiseImageInfo;

	writeComputeTextureInfo[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeComputeTextureInfo[4].pNext = NULL;
	writeComputeTextureInfo[4].dstSet = computeSet;
	writeComputeTextureInfo[4].dstBinding = 4;
	writeComputeTextureInfo[4].descriptorCount = 1;
	writeComputeTextureInfo[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeComputeTextureInfo[4].pImageInfo = &cloudCurlNoiseImageInfo;

	writeComputeTextureInfo[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeComputeTextureInfo[5].pNext = NULL;
	writeComputeTextureInfo[5].dstSet = computeSet;
	writeComputeTextureInfo[5].dstBinding = 5;
	writeComputeTextureInfo[5].descriptorCount = 1;
	writeComputeTextureInfo[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeComputeTextureInfo[5].pImageInfo = &weatherMapInfo;

	writeComputeTextureInfo[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeComputeTextureInfo[6].pNext = NULL;
	writeComputeTextureInfo[6].dstSet = computeSet;
	writeComputeTextureInfo[6].dstBinding = 6;
	writeComputeTextureInfo[6].descriptorCount = 1;
	writeComputeTextureInfo[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writeComputeTextureInfo[6].pImageInfo = &godRaysCreationDataTextureInfo;

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
void Renderer::WriteToAndUpdateCloudsDescriptorSets()
{
	//------------------------------------------
	//-------Cloud Pipeline DescriptorSets----
	//------------------------------------------
	VkDescriptorImageInfo currentFrameTextureInfo = {};
	currentFrameTextureInfo.imageLayout = currentFrameResultTexture->GetTextureLayout();
	currentFrameTextureInfo.imageView = currentFrameResultTexture->GetTextureImageView();
	currentFrameTextureInfo.sampler = currentFrameResultTexture->GetTextureSampler();
	// Texture Quad Reads in
	std::array<VkWriteDescriptorSet, 1> writeCloudSamplerInfo = {};
	writeCloudSamplerInfo[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeCloudSamplerInfo[0].pNext = NULL;
	writeCloudSamplerInfo[0].dstSet = cloudSet;
	writeCloudSamplerInfo[0].dstBinding = 0;
	writeCloudSamplerInfo[0].descriptorCount = 1;
	writeCloudSamplerInfo[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeCloudSamplerInfo[0].pImageInfo = &currentFrameTextureInfo;

	vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(writeCloudSamplerInfo.size()), writeCloudSamplerInfo.data(), 0, nullptr);
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
}
void Renderer::WriteToAndUpdatePostDescriptorSets()
{
	//------------------------------------------
	//-------- Post Process Descriptor Sets ----
	//------------------------------------------
	//God Rays
	// Texture God Rays Post Process Shader Writes To
	VkDescriptorImageInfo resultantImageTextureInfo = {};
	resultantImageTextureInfo.imageLayout = currentFrameResultTexture->GetTextureLayout();
	resultantImageTextureInfo.imageView = currentFrameResultTexture->GetTextureImageView();
	resultantImageTextureInfo.sampler = currentFrameResultTexture->GetTextureSampler();

	// God Rays Creation Data Texture
	VkDescriptorImageInfo godRaysDataTextureInfo = {};
	godRaysDataTextureInfo.imageLayout = godRaysCreationDataTexture->GetTextureLayout();
	godRaysDataTextureInfo.imageView = godRaysCreationDataTexture->GetTextureImageView();
	godRaysDataTextureInfo.sampler = godRaysCreationDataTexture->GetTextureSampler();

	std::array<VkWriteDescriptorSet, 2> writeGodRaysPassInfo = {};
	writeGodRaysPassInfo[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeGodRaysPassInfo[0].pNext = NULL;
	writeGodRaysPassInfo[0].dstSet = godRaysSet;
	writeGodRaysPassInfo[0].dstBinding = 0;
	writeGodRaysPassInfo[0].descriptorCount = 1;
	writeGodRaysPassInfo[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writeGodRaysPassInfo[0].pImageInfo = &resultantImageTextureInfo;

	writeGodRaysPassInfo[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeGodRaysPassInfo[1].pNext = NULL;
	writeGodRaysPassInfo[1].dstSet = godRaysSet;
	writeGodRaysPassInfo[1].dstBinding = 1;
	writeGodRaysPassInfo[1].descriptorCount = 1;
	writeGodRaysPassInfo[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeGodRaysPassInfo[1].pImageInfo = &godRaysDataTextureInfo;

	vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(writeGodRaysPassInfo.size()), writeGodRaysPassInfo.data(), 0, nullptr);

	// Final Pass
	// preFinalPassImage --> appply tone mapping and other effects to this
	VkDescriptorImageInfo preFinalPassImageInfo = {};
	preFinalPassImageInfo.imageLayout = currentFrameResultTexture->GetTextureLayout();
	preFinalPassImageInfo.imageView = currentFrameResultTexture->GetTextureImageView();
	preFinalPassImageInfo.sampler = currentFrameResultTexture->GetTextureSampler();

	std::array<VkWriteDescriptorSet, 1> writeFinalPassInfo = {};

	writeFinalPassInfo[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeFinalPassInfo[0].pNext = NULL;
	writeFinalPassInfo[0].dstSet = finalPassSet;
	writeFinalPassInfo[0].dstBinding = 0;
	writeFinalPassInfo[0].descriptorCount = 1;
	writeFinalPassInfo[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeFinalPassInfo[0].pImageInfo = &preFinalPassImageInfo;

	vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(writeFinalPassInfo.size()), writeFinalPassInfo.data(), 0, nullptr);
}

//----------------------------------------------
//-------------- Format Helper Functions -------
//----------------------------------------------
VkFormat Renderer::FindSupportedFormat(const std::vector<VkFormat>& candidates,
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

VkFormat Renderer::FindDepthFormat()
{
	return FindSupportedFormat({ VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT },
								VK_IMAGE_TILING_OPTIMAL,
								VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
								);
}

//----------------------------------------------
//------------- Create Compute Resources -------
//----------------------------------------------
void Renderer::CreateComputeResources()
{
	//To store the results of the compute shader that will be passed on to the frag shader
	currentFrameResultTexture = new Texture2D(device, window_width, window_height, VK_FORMAT_R8G8B8A8_UNORM);
	currentFrameResultTexture->createEmptyTexture(logicalDevice, physicalDevice, computeCommandPool);
	//Stores the results of the previous Frame
	previousFrameComputeResultTexture = new Texture2D(device, window_width, window_height, VK_FORMAT_R8G8B8A8_UNORM);
	previousFrameComputeResultTexture->createEmptyTexture(logicalDevice, physicalDevice, computeCommandPool);

	//Create the textures that will be passed to the compute shader to create clouds
	CreateCloudResources();
}

void Renderer::CreateCloudResources()
{
	// Low Frequency Cloud 3D Texture
	const std::string LowFreq_folder_path = "../../src/CloudScapes/textures/CloudTexturesUsed/LowFrequency/";
	const std::string LowFreq_textureBaseName = "LowFrequency";
	const std::string LowFreq_fileExtension = ".tga";
	cloudBaseShapeTexture = new Texture3D(device, 128, 128, 128, VK_FORMAT_R8G8B8A8_UNORM);
	cloudBaseShapeTexture->create3DTextureFromMany2DTextures(logicalDevice, computeCommandPool,
											LowFreq_folder_path, LowFreq_textureBaseName, LowFreq_fileExtension, 
											128, 4);

	// High Frequency Cloud 3D Texture //TODO Get actual High Frequncy Textures
	const std::string HighFreq_folder_path = "../../src/CloudScapes/textures/CloudTexturesUsed/HighFrequency/";
	const std::string HighFreq_textureBaseName = "HighFrequency";
	const std::string HighFreq_fileExtension = ".tga";
	cloudDetailsTexture = new Texture3D(device, 32, 32, 32, VK_FORMAT_R8G8B8A8_UNORM);
	cloudDetailsTexture->create3DTextureFromMany2DTextures(logicalDevice, computeCommandPool,
										HighFreq_folder_path, HighFreq_textureBaseName, HighFreq_fileExtension,
										32, 4);

	// Curl Noise 2D Texture
	const std::string curlNoiseTexture_path = "../../src/CloudScapes/textures/CloudTexturesUsed/curlNoise.png";
	cloudMotionTexture = new Texture2D(device, 128, 128, VK_FORMAT_R8G8B8A8_UNORM); //Need to pad an extra channel because R8G8B8 is not supported
	cloudMotionTexture->createTextureFromFile(logicalDevice, computeCommandPool, curlNoiseTexture_path, 4, 
											VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
											VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SAMPLER_ADDRESS_MODE_REPEAT, 16.0f);

	// Weather Map 2D Texture
	const std::string weatherMapTexture_path = "../../src/CloudScapes/textures/CloudTexturesUsed/weatherMap.png";
	weatherMapTexture = new Texture2D(device, 512, 512, VK_FORMAT_R8G8B8A8_UNORM); //Need to pad an extra channel because R8G8B8 is not supported
	weatherMapTexture->createTextureFromFile(logicalDevice, computeCommandPool, weatherMapTexture_path, 4,
											VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
											VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SAMPLER_ADDRESS_MODE_REPEAT, 16.0f);
}

//----------------------------------------------
//------------- Create Compute Resources -------
//----------------------------------------------
void Renderer::CreatePostProcessResources()
{
	//To store the results of the compute shader that will be passed on to the frag shader
	godRaysCreationDataTexture = new Texture2D(device, window_width, window_height, VK_FORMAT_R8G8B8A8_UNORM);
	godRaysCreationDataTexture->createEmptyTexture(logicalDevice, physicalDevice, computeCommandPool);
}

//----------------------------------------------
//--------------------- IMGUI ------------------
//----------------------------------------------

/*
	 Put this in for ImGui
 
	 Maybe put this instead?
	 #define ERR_GUARD_VULKAN(Expr) do { VkResult res__ = (Expr); if (res__ < 0) assert(0); } while(0)
 
 */
static void check_vk_result(VkResult err)
{
	if (err == 0) return;
	printf("VkResult %d\n", err);
	if (err < 0)
		abort();
}

void Renderer::ImGuiSetup(GLFWwindow* window)
{
	/*
	// Setup ImGui binding
	ImGui_ImplGlfwVulkan_Init_Data imgui_init_data = {};
	//imgui_init_data.allocator = g_vma_Allocator;	// We're using VmaAllocator g_vma_Allocator instead?
	imgui_init_data.gpu = physicalDevice;
	imgui_init_data.device = logicalDevice;
	imgui_init_data.render_pass = renderPass;
	//imgui_init_data.pipeline_cache = 
	imgui_init_data.descriptor_pool = descriptorPool;
	imgui_init_data.check_vk_result = check_vk_result;
	ImGui_ImplGlfwVulkan_Init(window, true, &imgui_init_data);\
	*/

	// Reference --> Vulkan Example --> ImGui_ImplGlfwVulkan_NewFrame()
	ImGuiIO& io = ImGui::GetIO();
	int w, h;
	int display_w, display_h;
	glfwGetWindowSize(window, &w, &h);
	glfwGetFramebufferSize(window, &display_w, &display_h);
	io.DisplaySize = ImVec2((float)w, (float)h);
	io.DisplayFramebufferScale = ImVec2(w > 0 ? ((float)display_w / w) : 0,
										h > 0 ? ((float)display_h / h) : 0);

	io.RenderDrawListsFn = NULL;

	// Build the font atlas texture, then load the texture pixels into graphics memory
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	io.DeltaTime = 1.0f / 60.0f;

	double mouse_x, mouse_y;
	glfwGetCursorPos(window, &mouse_x, &mouse_y);
	io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);
	io.MouseDown[0] = glfwGetMouseButton(window, 0);
	io.MouseDown[1] = glfwGetMouseButton(window, 1);
	
}

void Renderer::ImGuiCreation()
{
	// 1. Show a simple window.
	// Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug".

	ImGui::NewFrame();
	ImGui::Text("New ImGui window");

	//ImGui::ShowTestWindow();
}

void Renderer::ImGuiRender()
{
	ImGui::Render();
}