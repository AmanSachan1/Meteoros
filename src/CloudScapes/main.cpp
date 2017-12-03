#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <vulkan/vulkan.h>
#include "VulkanInstance.h"
#include "Window.h"
#include "Renderer.h"

#include "Scene.h"
#include "Camera.h"
#include "Image.h"

VulkanDevice* device; // manages both the logical device (VkDevice) and the physical Device (VkPhysicalDevice)
VulkanSwapChain* swapChain;
Renderer* renderer;

Camera* camera;

int window_height = 562.5;
int window_width = 1000;

namespace 
{
	void resizeCallback(GLFWwindow* window, int width, int height) 
	{
		if (width == 0 || height == 0) return;

		vkDeviceWaitIdle(device->GetVkDevice());
		swapChain->Recreate();
		renderer->RecreateFrameResources();
	}

	bool leftMouseDown = false;
	double previousX = 0.0;
	double previousY = 0.0;
	float deltaForRotation = 0.01;
	float deltaForMovement = 0.001;

	void keyboardInputs(GLFWwindow* window)
	{
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
			glfwSetWindowShouldClose(window, true);
		}			

		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
			camera->TranslateAlongLook(deltaForMovement);
		}			
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
			camera->TranslateAlongLook(-deltaForMovement);
		}

		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
			camera->TranslateAlongRight(-deltaForMovement);
		}
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
			camera->TranslateAlongRight(deltaForMovement);
		}			

		if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
			camera->TranslateAlongUp(deltaForMovement);
		}		
		if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
			camera->TranslateAlongUp(-deltaForMovement);
		}

		if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
			camera->RotateAboutRight(deltaForRotation);
		}
		if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
			camera->RotateAboutRight(-deltaForRotation);
		}
		if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
			camera->RotateAboutUp(deltaForRotation);
		}
		if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
			camera->RotateAboutUp(-deltaForRotation);
		}

		camera->UpdateBuffer();
		camera->CopyToGPUMemory();
	}
	
	void mouseDownCallback(GLFWwindow* window, int button, int action, int mods) 
	{
		if (button == GLFW_MOUSE_BUTTON_LEFT) {
			if (action == GLFW_PRESS) {
				leftMouseDown = true;
				glfwGetCursorPos(window, &previousX, &previousY);
			}
			else if (action == GLFW_RELEASE) {
				leftMouseDown = false;
			}
		}
	}

	void mouseMoveCallback(GLFWwindow* window, double xPosition, double yPosition) 
	{
		if (leftMouseDown) 
		{
			double sensitivity = 0.05;
			float deltaX = static_cast<float>((previousX - xPosition) * sensitivity);
			float deltaY = static_cast<float>((previousY - yPosition) * sensitivity);
			previousX = xPosition;
			previousY = yPosition;

			camera->RotateAboutUp(deltaX);
			camera->RotateAboutRight(deltaY);

			camera->UpdateBuffer();
			camera->CopyToGPUMemory();
		}
	}

	void scrollCallback(GLFWwindow*, double, double yoffset)
	{
		camera->TranslateAlongLook(static_cast<float>(yoffset) * 0.05f);
		camera->UpdateBuffer();
		camera->CopyToGPUMemory();
	}
}

int main(int argc, char** argv) 
{
    static constexpr char* applicationName = "CIS 565: Final Project -- Vertical Multiple Dusks";
    InitializeWindow(window_width, window_height, applicationName);

    unsigned int glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	// Vulkan Instance
	VulkanInstance* instance = new VulkanInstance(applicationName, glfwExtensionCount, glfwExtensions);

	// Drawing Surface, i.e. window where things are rendered to
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance->GetVkInstance(), GetGLFWWindow(), nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface");
    }

	// physical Device --> GPU
	// TransferBit tells Vulkan that we can transfer data between CPU and GPU
    instance->PickPhysicalDevice({ VK_KHR_SWAPCHAIN_EXTENSION_NAME }, 
								 QueueFlagBit::GraphicsBit | QueueFlagBit::TransferBit | QueueFlagBit::ComputeBit | QueueFlagBit::PresentBit, 
								 surface);

	// Device --> Logical Device: Communicates with the Physical Device and generally acts as an interface to the Physical device
	// Reference: https://vulkan-tutorial.com/Drawing_a_triangle/Presentation
	// QueueFlagBit::PresentBit --> Vulkan is trying to determine if the Queue we  will setup to send commands through can 
	// actually 'present' images, i.e display them
	// QueueFlagBit::PresentBit --> Vulkan is trying to determine if we can make use of the window surface we just created, i.e. draw on the window
    device = instance->CreateDevice(QueueFlagBit::GraphicsBit | QueueFlagBit::TransferBit | QueueFlagBit::ComputeBit | QueueFlagBit::PresentBit);
    
	swapChain = device->CreateSwapChain(surface);

	camera = new Camera(device, glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 1.0f), 
						window_width, window_height, 45.0f, window_width / window_height, 0.1f, 1000.0f);

	Scene* scene = new Scene(device);

	renderer = new Renderer(device, instance->GetPhysicalDevice(), swapChain, scene, camera, static_cast<uint32_t>(window_width), static_cast<uint32_t>(window_height));

	glfwSetWindowSizeCallback(GetGLFWWindow(), resizeCallback);
	glfwSetMouseButtonCallback(GetGLFWWindow(), mouseDownCallback);
	glfwSetScrollCallback(GetGLFWWindow(), scrollCallback);
	glfwSetCursorPosCallback(GetGLFWWindow(), mouseMoveCallback);

	// Reference: https://vulkan-tutorial.com/Drawing_a_triangle/Drawing/Rendering_and_presentation
    while (!ShouldQuit()) 
	{
		glfwPollEvents();
		scene->UpdateTime();
		keyboardInputs(GetGLFWWindow());
		renderer->Frame();
    }// end while loop

    // Wait for the device to finish executing before cleanup
    vkDeviceWaitIdle(device->GetVkDevice());

	//---------------------
	//------ CleanUp ------
	//---------------------	
	delete renderer;
	delete scene;
	delete camera;

    delete swapChain;
    vkDestroySurfaceKHR(instance->GetVkInstance(), surface, nullptr);
    delete device;
    delete instance;
    DestroyWindow();
}