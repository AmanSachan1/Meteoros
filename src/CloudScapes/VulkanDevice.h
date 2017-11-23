#pragma once

#include <array>
#include <vulkan/vulkan.h>
#include "Forward.h"
#include "VulkanInstance.h"
#include "SwapChain.h"

class VulkanDevice 
{
	/*
		In principle, private and protected members of a class cannot be accessed
		from outside the same class in which they are declared. 
		However, this rule does not apply to "friends".

		Friends are functions or classes declared with the friend keyword.
		A non-member function can access the private and protected members of a 
		class if it is declared a friend of that class.
	*/
    friend class VulkanInstance;

public:
    VulkanSwapChain* CreateSwapChain(VkSurfaceKHR surface);
    VulkanInstance* GetInstance();
    VkDevice GetVulkanDevice();
    VkQueue GetQueue(QueueFlags flag);
	unsigned int GetQueueIndex(QueueFlags flag);
    ~VulkanDevice();

private:
    using Queues = std::array<VkQueue, sizeof(QueueFlags)>;
    
    VulkanDevice() = delete;
    VulkanDevice(VulkanInstance* instance, VkDevice vkDevice, Queues queues);

    VulkanInstance* instance;
    VkDevice vkDevice;
    Queues queues;
};