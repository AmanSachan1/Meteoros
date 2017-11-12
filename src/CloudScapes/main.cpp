#include <stdexcept>
#include <vulkan/vulkan.h>
#include "window.h"
#include "vulkan_instance.h"
#include "vulkan_shader_module.h"

VulkanInstance* instance;
VulkanDevice* device;
VulkanSwapChain* swapchain;

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

// Reference: https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Fixed_functions
VkPipelineLayout CreateGraphicsPipelineLayout()
{
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 0;
	pipelineLayoutInfo.pSetLayouts = nullptr;
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

	// -------- Vertex input --------
	// Because we're hard coding the vertex data directly in the vertex shader, we'll fill in this structure to specify 
	// that there is no vertex data to load for now. We'll write the vertex buffers later.
	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr;

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

std::vector<VkFramebuffer> CreateFrameBuffers(VkRenderPass renderPass) {
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
	int window_height = 640;
	int window_width = 480;
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
    instance->PickPhysicalDevice({ VK_KHR_SWAPCHAIN_EXTENSION_NAME }, QueueFlagBit::GraphicsBit | QueueFlagBit::PresentBit, surface);

	// Device --> Logical Device: Communicates with the Physical Device and generally acts as an interface to the Physical device
	// Reference: https://vulkan-tutorial.com/Drawing_a_triangle/Presentation
	// QueueFlagBit::PresentBit --> Vulkan is trying to determine if the Queue we  will setup to send commands through can 
	// actually 'present' images, i.e display them
	// QueueFlagBit::PresentBit --> Vulkan is trying to determine if we can make use of the window surface we just created, i.e. draw on the window
    device = instance->CreateDevice(QueueFlagBit::GraphicsBit | QueueFlagBit::PresentBit);
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
    if (vkCreateCommandPool(device->GetVulkanDevice(), &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }

    VkRenderPass renderPass = CreateRenderPass();
	VkPipelineLayout graphicsPipelineLayout = CreateGraphicsPipelineLayout();
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

        vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		// VK_SUBPASS_CONTENTS_INLINE: The render pass commands will be embedded in the primary command
		// buffer itself and no secondary command buffers will be executed.

		// Bind the graphics pipeline
		vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		// Draw
		vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);
		/*
			vkCmdDraw has the following parameters, aside from the command buffer:
				vertexCount: Even though we don't have a vertex buffer, we technically still have 3 vertices to draw.
				instanceCount: Used for instanced rendering, use 1 if you're not doing that.
				firstVertex: Used as an offset into the vertex buffer, defines the lowest value of gl_VertexIndex.
				firstInstance: Used as an offset for instanced rendering, defines the lowest value of gl_InstanceIndex.
		*/

        vkCmdEndRenderPass(commandBuffers[i]);

		//---------- End Recording ----------
        if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to record command buffer");
        }
    }

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
    }

    // Wait for the device to finish executing before cleanup
    vkDeviceWaitIdle(device->GetVulkanDevice());

    vkDestroyRenderPass(device->GetVulkanDevice(), renderPass, nullptr);
    vkFreeCommandBuffers(device->GetVulkanDevice(), commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
    for (size_t i = 0; i < frameBuffers.size(); i++) {
        vkDestroyFramebuffer(device->GetVulkanDevice(), frameBuffers[i], nullptr);
    }
    delete swapchain;

    vkDestroyCommandPool(device->GetVulkanDevice(), commandPool, nullptr);
    vkDestroySurfaceKHR(instance->GetVkInstance(), surface, nullptr);
    delete device;
    delete instance;

    DestroyWindow();
}