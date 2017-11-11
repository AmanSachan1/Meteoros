#include <stdexcept>
#include <set>
#include <vector>
#include "vulkan_instance.h"

#ifdef NDEBUG
const bool ENABLE_VALIDATION = false;
#else
const bool ENABLE_VALIDATION = true;
#endif

namespace {
    const std::vector<const char*> validationLayers = {
        "VK_LAYER_LUNARG_standard_validation"
    };

    // Get the required list of extensions based on whether validation layers are enabled
    std::vector<const char*> getRequiredExtensions() {
        std::vector<const char*> extensions;

        if (ENABLE_VALIDATION) {
            extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        }

        return extensions;
    }

    // Callback function to allow messages from validation layers to be received
    VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugReportFlagsEXT flags,
        VkDebugReportObjectTypeEXT objType,
        uint64_t obj,
        size_t location,
        int32_t code,
        const char* layerPrefix,
        const char* msg,
        void *userData) {

        fprintf(stderr, "Validation layer: %s\n", msg);
        return VK_FALSE;
    }
}

VulkanInstance::VulkanInstance(const char* applicationName, unsigned int additionalExtensionCount, const char** additionalExtensions) {
    // --- Specify details about our application ---
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = applicationName;
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;
    
    // --- Create Vulkan instance ---
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // Get extensions necessary for Vulkan to interface with GLFW
    auto extensions = getRequiredExtensions();
    for (unsigned int i = 0; i < additionalExtensionCount; ++i) {
        extensions.push_back(additionalExtensions[i]);
    }
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    // Specify global validation layers
    if (ENABLE_VALIDATION) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    // Create instance
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create instance");
    }

    initDebugReport();
}

VkInstance VulkanInstance::GetVkInstance() {
    return instance;
}

VkPhysicalDevice VulkanInstance::GetPhysicalDevice() {
    return physicalDevice;
}

const VkSurfaceCapabilitiesKHR& VulkanInstance::GetSurfaceCapabilities() const {
    return surfaceCapabilities;
}

const QueueFamilyIndices& VulkanInstance::GetQueueFamilyIndices() const {
    return queueFamilyIndices;
}

const std::vector<VkSurfaceFormatKHR>& VulkanInstance::GetSurfaceFormats() const {
    return surfaceFormats;
}

const std::vector<VkPresentModeKHR>& VulkanInstance::GetPresentModes() const {
    return presentModes;
}

uint32_t VulkanInstance::GetMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties) const {
    // Iterate over all memory types available for the device used in this example
    for (uint32_t i = 0; i < deviceMemoryProperties.memoryTypeCount; i++) {
        if ((typeBits & 1) == 1) {
            if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        typeBits >>= 1;
    }
    throw std::runtime_error("Could not find a suitable memory type!");
}

void VulkanInstance::initDebugReport() {
    if (ENABLE_VALIDATION) {
        // Specify details for callback
        VkDebugReportCallbackCreateInfoEXT createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
        createInfo.pfnCallback = debugCallback;

        if ([&]() {
            auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
            if (func != nullptr) {
                return func(instance, &createInfo, nullptr, &debugReportCallback);
            }
            else {
                return VK_ERROR_EXTENSION_NOT_PRESENT;
            }
        }() != VK_SUCCESS) {
            throw std::runtime_error("Failed to set up debug callback");
        }
    }
}


namespace {
    QueueFamilyIndices checkDeviceQueueSupport(VkPhysicalDevice device, QueueFlagBits requiredQueues, VkSurfaceKHR surface = VK_NULL_HANDLE) {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        VkQueueFlags requiredVulkanQueues = 0;
        if (requiredQueues[QueueFlags::Graphics]) {
            requiredVulkanQueues |= VK_QUEUE_GRAPHICS_BIT;
        }
        if (requiredQueues[QueueFlags::Compute]) {
            requiredVulkanQueues |= VK_QUEUE_COMPUTE_BIT;
        }
        if (requiredQueues[QueueFlags::Transfer]) {
            requiredVulkanQueues |= VK_QUEUE_TRANSFER_BIT;
        }

        QueueFamilyIndices indices = {};
        indices.fill(-1);
        VkQueueFlags supportedQueues = 0;
        bool needsPresent = requiredQueues[QueueFlags::Present];
        bool presentSupported = false;

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            if (queueFamily.queueCount > 0) {
                supportedQueues |= queueFamily.queueFlags;
            }

            if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices[QueueFlags::Graphics] = i;
            }

            if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
                indices[QueueFlags::Compute] = i;
            }

            if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) {
                indices[QueueFlags::Transfer] = i;
            }

            if (needsPresent) {
                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
                if (queueFamily.queueCount > 0 && presentSupport) {
                    presentSupported = true;
                    indices[QueueFlags::Present] = i;
                }
            }

            if ((requiredVulkanQueues & supportedQueues) == requiredVulkanQueues && (!needsPresent || presentSupported)) {
                break;
            }

            i++;
        }

        return indices;
    }

    // Check the physical device for specified extension support
    bool checkDeviceExtensionSupport(VkPhysicalDevice device, std::vector<const char*> requiredExtensions) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensionSet(requiredExtensions.begin(), requiredExtensions.end());

        for (const auto& extension : availableExtensions) {
            requiredExtensionSet.erase(extension.extensionName);
        }

        return requiredExtensionSet.empty();
    }
}

void VulkanInstance::PickPhysicalDevice(std::vector<const char*> deviceExtensions, QueueFlagBits requiredQueues, VkSurfaceKHR surface) {
    // List the graphics cards on the machine
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    // Evaluate each GPU and check if it is suitable
    for (const auto& device : devices) {
        bool queueSupport = true;
        queueFamilyIndices = checkDeviceQueueSupport(device, requiredQueues, surface);
        for (unsigned int i = 0; i < requiredQueues.size(); ++i) {
            if (requiredQueues[i]) {
                queueSupport &= (queueFamilyIndices[i] >= 0);
            }
        }

        if (requiredQueues[QueueFlags::Present]) {
            // Get basic surface capabilities
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &surfaceCapabilities);

            // Query supported surface formats
            uint32_t formatCount;
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

            if (formatCount != 0) {
                surfaceFormats.resize(formatCount);
                vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, surfaceFormats.data());
            }

            // Query supported presentation modes
            uint32_t presentModeCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

            if (presentModeCount != 0) {
                presentModes.resize(presentModeCount);
                vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, presentModes.data());
            }
        }

        if (queueSupport &&
            checkDeviceExtensionSupport(device, deviceExtensions) &&
            (!requiredQueues[QueueFlags::Present] || (!surfaceFormats.empty() && ! presentModes.empty()))
        ) {
            physicalDevice = device;
            break;
        }
    }

    this->deviceExtensions = deviceExtensions;
    
    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable GPU");
    }

    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);
}

VulkanDevice* VulkanInstance::CreateDevice(QueueFlagBits requiredQueues) {
    std::set<int> uniqueQueueFamilies;
    bool queueSupport = true;
    for (unsigned int i = 0; i < requiredQueues.size(); ++i) {
        if (requiredQueues[i]) {
            queueSupport &= (queueFamilyIndices[i] >= 0);
            uniqueQueueFamilies.insert(queueFamilyIndices[i]);
        }
    }

    if (!queueSupport) {
        throw std::runtime_error("Device does not support requested queues");
    }

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    float queuePriority = 1.0f;
    for (int queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // --- Specify the set of device features used ---
    VkPhysicalDeviceFeatures deviceFeatures = {};

    // --- Create logical device ---
    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();

    createInfo.pEnabledFeatures = &deviceFeatures;

    // Enable device-specific extensions and validation layers
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (ENABLE_VALIDATION) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    VkDevice vkDevice;
    // Create logical device
    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &vkDevice) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device");
    }

    VulkanDevice::Queues queues;
    for (unsigned int i = 0; i < requiredQueues.size(); ++i) {
        if (requiredQueues[i]) {
            vkGetDeviceQueue(vkDevice, queueFamilyIndices[i], 0, &queues[i]);
        }
    }

    return new VulkanDevice(this, vkDevice, queues);
}

VulkanInstance::~VulkanInstance() {
    if (ENABLE_VALIDATION) {
        auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
        if (func != nullptr) {
            func(instance, debugReportCallback, nullptr);
        }
    }

    vkDestroyInstance(instance, nullptr);
}