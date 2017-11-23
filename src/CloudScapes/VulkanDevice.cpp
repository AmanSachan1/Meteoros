#include "VulkanDevice.h"

VulkanDevice::VulkanDevice(VulkanInstance* instance, VkDevice vkDevice, Queues queues)
  : instance(instance), vkDevice(vkDevice), queues(queues) 
{}

VulkanDevice::~VulkanDevice()
{
	vkDestroyDevice(vkDevice, nullptr);
}

VulkanInstance* VulkanDevice::GetInstance() 
{
    return instance;
}

VkDevice VulkanDevice::GetVkDevice() 
{
    return vkDevice;
}

VkQueue VulkanDevice::GetQueue(QueueFlags flag) 
{
    return queues[flag];
}

unsigned int VulkanDevice::GetQueueIndex(QueueFlags flag) 
{
	return GetInstance()->GetQueueFamilyIndices()[flag];
}

VulkanSwapChain* VulkanDevice::CreateSwapChain(VkSurfaceKHR surface) 
{
    return new VulkanSwapChain(this, surface);
}