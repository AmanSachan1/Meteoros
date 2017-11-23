#pragma once

#include "VulkanDevice.h"
#include "SwapChain.h"
#include "Camera.h"
#include "Scene.h"

class Renderer 
{
public:
	Renderer() = delete; // To enforce the creation of a the type of renderer we want without leaving the vulkan device, vulkan swapchain, etc as assumptions or nullptrs
	Renderer(VulkanDevice* device, VulkanSwapChain* swapChain, Scene* scene, Camera* camera);
	~Renderer();

private:
};