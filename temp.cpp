#pragma once



#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <iostream>

#include "Window.h"
#include "VulkanInstance.h"

VulkanInstance* instance;
VulkanDevice* device; // manages both the logical device (VkDevice) and the physical Device (VkPhysicalDevice)
VulkanSwapChain* swapchain;
Renderer* renderer;

VkImage depthImage;
VkDeviceMemory depthImageMemory;
VkImageView depthImageView;

VkCommandPool commandPool;

Camera* camera;
glm::mat4* mappedCameraView;
int window_height = 480;
int window_width = 640;

namespace 
{
	bool buttons[GLFW_MOUSE_BUTTON_LAST + 1] = { 0 };

	void mouseButtonCallback(GLFWwindow*, int button, int action, int) 
	{
		buttons[button] = (action == GLFW_PRESS);
	}

	void cursorPosCallback(GLFWwindow*, double mouseX, double mouseY) 
	{
		static double oldX, oldY;
		float dX = static_cast<float>(mouseX - oldX);
		float dY = static_cast<float>(mouseY - oldY);
		oldX = mouseX;
		oldY = mouseY;

		if (buttons[2] || (buttons[0] && buttons[1])) 
		{
			camera->pan(-dX * 0.002f, dY * -0.002f);
			memcpy(mappedCameraView, &camera->view(), sizeof(glm::mat4));
		}
		else if (buttons[0]) 
		{
			camera->rotate(dX * -0.01f, dY * -0.01f);
			memcpy(mappedCameraView, &camera->view(), sizeof(glm::mat4));
		}
		else if (buttons[1]) 
		{
			camera->zoom(dY * -0.005f);
			memcpy(mappedCameraView, &camera->view(), sizeof(glm::mat4));
		}
	}

	void scrollCallback(GLFWwindow*, double, double yoffset) 
	{
		camera->zoom(static_cast<float>(yoffset) * 0.04f);
		memcpy(mappedCameraView, &camera->view(), sizeof(glm::mat4));
	}
}



int main(int argc, char** argv) 
{
    static constexpr char* applicationName = "CIS 565: Final Project -- Vertical Multiple Dusks";
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

    // Record command buffers, one for each frame of the swapchain
    for (unsigned int i = 0; i < commandBuffers.size(); ++i) 
	{
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
		std::array<VkClearValue, 2> clearValues = {};
		clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f }; //black
		clearValues[1].depthStencil = { 1.0f, 0 }; //far plane

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

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