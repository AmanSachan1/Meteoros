#pragma once

#include <vector>
#include <vulkan/vulkan.h>

// Reference: https://github.com/SaschaWillems/Vulkan/blob/master/base/VulkanInitializers.hpp
namespace VulkanInitializers
{
	//--------------------------------------------------------
	//				Shader Program Loading
	//--------------------------------------------------------

	inline VkPipelineShaderStageCreateInfo loadShader(VkShaderStageFlagBits shaderStageBits ,VkShaderModule shadermodule)
	{
		VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {};
		shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageCreateInfo.stage = shaderStageBits;
		shaderStageCreateInfo.module = shadermodule;
		shaderStageCreateInfo.pName = "main";

		return shaderStageCreateInfo;
	}

	//--------------------------------------------------------
	//				PipeLine Creation Initializers
	//--------------------------------------------------------

	inline VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo()
	{
		// -------- Vertex input --------
		// Because we're hard coding the vertex data directly in the vertex shader, we'll fill in this structure to specify 
		// that there is no vertex data to load for now. We'll write the vertex buffers later.
		VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo{};
		pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		return pipelineVertexInputStateCreateInfo;
	}

	inline VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo( VkPrimitiveTopology topology,
																						VkPipelineInputAssemblyStateCreateFlags flags,
																						VkBool32 primitiveRestartEnable)
	{
		// -------- Input assembly --------
		// The VkPipelineInputAssemblyStateCreateInfo struct describes two things: what kind of geometry will be drawn 
		// from the vertices and if primitive restart should be enabled.
		VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo{};
		pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		pipelineInputAssemblyStateCreateInfo.topology = topology;
		pipelineInputAssemblyStateCreateInfo.flags = flags;
		pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = primitiveRestartEnable;
		return pipelineInputAssemblyStateCreateInfo;
	}

	inline VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo( VkPolygonMode polygonMode,
																						VkCullModeFlags cullMode, VkFrontFace frontFace,
																						VkPipelineRasterizationStateCreateFlags flags = 0)
	{
		// -------- Rasterization State --------
		// The rasterizer takes the geometry that is shaped by the vertices from the vertex shader and turns
		// it into fragments to be colored by the fragment shader.
		// It also performs depth testing, face culling and the scissor test, and it can be configured to output 
		// fragments that fill entire polygons or just the edges (wireframe rendering). All this is configured 
		// using the VkPipelineRasterizationStateCreateInfo structure.
		VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo{};
		pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
		pipelineRasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE; // If rasterizerDiscardEnable is set to VK_TRUE, then geometry never 
															// passes through the rasterizer stage. This basically disables any output to the framebuffer.
		pipelineRasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;  // The rasterizer can alter the depth values by adding a constant value or biasing 
															// them based on a fragment's slope. This is sometimes used for shadow mapping, but we won't 
															// be using it.Just set depthBiasEnable to VK_FALSE.
		pipelineRasterizationStateCreateInfo.polygonMode = polygonMode;
		pipelineRasterizationStateCreateInfo.cullMode = cullMode;
		pipelineRasterizationStateCreateInfo.frontFace = frontFace;
		pipelineRasterizationStateCreateInfo.flags = flags;
		pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;
		return pipelineRasterizationStateCreateInfo;
	}

	inline VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState( VkColorComponentFlags colorWriteMask,
																				  VkBool32 blendEnable)
	{
		// -------- Color Blending ---------
		// --> Color Blending is usually for transparency
		// --> Configuration per attached framebuffer
		VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState{};
		pipelineColorBlendAttachmentState.colorWriteMask = colorWriteMask;
		pipelineColorBlendAttachmentState.blendEnable = blendEnable;
		return pipelineColorBlendAttachmentState;
	}

	inline VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo( uint32_t attachmentCount,
																				  const VkPipelineColorBlendAttachmentState * pAttachments)
	{
		// --> Global color blending settings
		VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo{};
		pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		pipelineColorBlendStateCreateInfo.attachmentCount = attachmentCount;
		pipelineColorBlendStateCreateInfo.pAttachments = pAttachments;
		return pipelineColorBlendStateCreateInfo;
	}

	inline VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo( VkBool32 depthTestEnable, VkBool32 depthWriteEnable,
																					  VkCompareOp depthCompareOp)
	{
		//The depthTestEnable field specifies if the depth of new fragments should be compared to the 
		//depth buffer to see if they should be discarded. The depthWriteEnable field specifies if the 
		//new depth of fragments that pass the depth test should actually be written to the depth buffer. 
		//This is useful for drawing transparent objects. They should be compared to the previously rendered 
		//opaque objects, but not cause further away transparent objects to not be drawn.
		VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo{};
		pipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		pipelineDepthStencilStateCreateInfo.depthTestEnable = depthTestEnable;
		pipelineDepthStencilStateCreateInfo.depthWriteEnable = depthWriteEnable;
		pipelineDepthStencilStateCreateInfo.depthCompareOp = depthCompareOp; // specifies the comparison that is performed to keep or discard fragments.
		pipelineDepthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
		pipelineDepthStencilStateCreateInfo.minDepthBounds = 0.0f;
		pipelineDepthStencilStateCreateInfo.maxDepthBounds = 1.0f;
		pipelineDepthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
		pipelineDepthStencilStateCreateInfo.front = pipelineDepthStencilStateCreateInfo.back;
		pipelineDepthStencilStateCreateInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
		return pipelineDepthStencilStateCreateInfo;
	}

	inline VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo( uint32_t viewportCount, uint32_t scissorCount, 
																			  VkPipelineViewportStateCreateFlags flags = 0)
	{
		// Now this viewport and scissor rectangle need to be combined into a viewport state using the 
		// VkPipelineViewportStateCreateInfo struct. It is possible to use multiple viewports and scissor
		// rectangles on some graphics cards, so its members reference an array of them. Using multiple requires 
		// enabling a GPU feature (see logical device creation).
		VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo{};
		pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		pipelineViewportStateCreateInfo.viewportCount = viewportCount;
		pipelineViewportStateCreateInfo.scissorCount = scissorCount;
		pipelineViewportStateCreateInfo.flags = flags;
		return pipelineViewportStateCreateInfo;
	}

	inline VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo( VkSampleCountFlagBits rasterizationSamples,
																					VkPipelineMultisampleStateCreateFlags flags = 0)
	{
		// -------- Multisampling --------
		// The VkPipelineMultisampleStateCreateInfo struct configures multisampling, which is one of the ways to perform anti-aliasing. 
		// It works by combining the fragment shader results of multiple polygons that rasterize to the same pixel. This mainly occurs 
		// along edges, which is also where the most noticeable aliasing artifacts occur. Because it doesn't need to run the fragment 
		// shader multiple times if only one polygon maps to a pixel, it is significantly less expensive than simply rendering to a 
		// higher resolution and then downscaling. Enabling it requires enabling a GPU feature.
		VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo{};
		pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		pipelineMultisampleStateCreateInfo.rasterizationSamples = rasterizationSamples;
		pipelineMultisampleStateCreateInfo.flags = flags;
		return pipelineMultisampleStateCreateInfo;
	}

	inline VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo( const std::vector<VkDynamicState>& pDynamicStates,
																			VkPipelineDynamicStateCreateFlags flags = 0)
	{
		VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo{};
		pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		pipelineDynamicStateCreateInfo.pDynamicStates = pDynamicStates.data();
		pipelineDynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(pDynamicStates.size());
		pipelineDynamicStateCreateInfo.flags = flags;
		return pipelineDynamicStateCreateInfo;
	}

	inline VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo( VkPipelineLayout layout, VkRenderPass renderPass,
																	VkPipelineCreateFlags flags = 0)
	{
		VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.layout = layout;
		pipelineCreateInfo.renderPass = renderPass;
		pipelineCreateInfo.flags = flags;
		pipelineCreateInfo.basePipelineIndex = VK_NULL_HANDLE; // Vulkan allows you to create a new graphics pipeline by deriving from an existing pipeline. 
															   // We aren't doing this.
		pipelineCreateInfo.basePipelineIndex = -1;
		return pipelineCreateInfo;
	}

	inline void createPipelineCache( VkDevice& logicalDevice, VkPipelineCache &pipelineCache )
	{
		VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
		pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		if (vkCreatePipelineCache(logicalDevice, &pipelineCacheCreateInfo, nullptr, &pipelineCache) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create pipeline cache");
		}
	}

	//--------------------------------------------------------
	//			Descriptor Sets and Descriptor Layouts
	// Reference: https://vulkan-tutorial.com/Uniform_buffers
	//--------------------------------------------------------
	
	inline VkDescriptorSetLayout CreateDescriptorSetLayout(VkDevice& logicalDevice, std::vector<VkDescriptorSetLayoutBinding> layoutBindings)
	{
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
		descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorSetLayoutCreateInfo.pNext = nullptr;
		descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
		descriptorSetLayoutCreateInfo.pBindings = layoutBindings.data();

		VkDescriptorSetLayout descriptorSetLayout;
		vkCreateDescriptorSetLayout(logicalDevice, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout);
		return descriptorSetLayout;
	}

	inline VkDescriptorSet CreateDescriptorSet(VkDevice& logicalDevice, VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout)
	{
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &descriptorSetLayout;

		VkDescriptorSet descriptorSet;
		vkAllocateDescriptorSets(logicalDevice, &allocInfo, &descriptorSet);
		return descriptorSet;
	}
};