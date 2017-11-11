#include "vulkan_device.h"

VulkanDevice::VulkanDevice(VulkanInstance* instance, VkDevice vkDevice, Queues queues)
  : instance(instance), vkDevice(vkDevice), queues(queues) {

}

VulkanInstance* VulkanDevice::GetInstance() {
    return instance;
}

VkDevice VulkanDevice::GetVulkanDevice() {
    return vkDevice;
}

VkQueue VulkanDevice::GetQueue(QueueFlags flag) {
    return queues[flag];
}

VulkanSwapChain* VulkanDevice::CreateSwapChain(VkSurfaceKHR surface) {
    return new VulkanSwapChain(this, surface);
}

VulkanDevice::~VulkanDevice() {
    vkDestroyDevice(vkDevice, nullptr);
}