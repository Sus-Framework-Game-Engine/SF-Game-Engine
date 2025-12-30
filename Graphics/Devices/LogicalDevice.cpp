#include "LogicalDevice.hpp"

#include <Graphics/Graphics.hpp>
#include "Instance.hpp"
#include "PhysicalDevice.hpp"
#include <unordered_set>

namespace SF::Engine
{
    const std::vector<const char *> LogicalDevice::DeviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    LogicalDevice::LogicalDevice(const Instance &instance, const PhysicalDevice &physicalDevice)
        : instance(instance), physicalDevice(physicalDevice)
    {
        CreateQueueIndices();
        CreateLogicalDevice();
    }

    LogicalDevice::~LogicalDevice()
    {
        RenderSystem::CheckVkResult(vkDeviceWaitIdle(logicalDevice));
        vkDestroyDevice(logicalDevice, nullptr);
    }

    void LogicalDevice::CreateQueueIndices()
    {
        uint32_t deviceQueueFamilyPropertyCount;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &deviceQueueFamilyPropertyCount, nullptr);
        std::vector<VkQueueFamilyProperties> deviceQueueFamilyProperties(deviceQueueFamilyPropertyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &deviceQueueFamilyPropertyCount, deviceQueueFamilyProperties.data());

        std::optional<uint32_t> GraphicsFamily, presentFamily, computeFamily, transferFamily;
        std::optional<uint32_t> dedicatedTransferFamily, dedicatedComputeFamily;

        for (uint32_t i = 0; i < deviceQueueFamilyPropertyCount; i++)
        {
            const auto &queueFamily = deviceQueueFamilyProperties[i];

            // Check for RenderSystem support
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                if (!GraphicsFamily)
                {
                    GraphicsFamily = i;
                    this->GraphicsFamily = i;
                    supportedQueues |= VK_QUEUE_GRAPHICS_BIT;
                }
            }

            // Check for present support
            // If you have a surface, uncomment and use this:
            // VkBool32 presentSupport = false;
            // vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
            // if (queueFamily.queueCount > 0 && presentSupport)

            // For now, assuming RenderSystem queue can present
            if (queueFamily.queueCount > 0 && (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
            {
                if (!presentFamily)
                {
                    presentFamily = i;
                    this->presentFamily = i;
                }
            }

            // Check for compute support
            if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                if (!computeFamily)
                {
                    computeFamily = i;
                    this->computeFamily = i;
                    supportedQueues |= VK_QUEUE_COMPUTE_BIT;
                }

                // Look for dedicated compute queue (compute but not RenderSystem)
                if (!dedicatedComputeFamily &&
                    !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
                {
                    dedicatedComputeFamily = i;
                }
            }

            // Check for transfer support
            if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
            {
                if (!transferFamily)
                {
                    transferFamily = i;
                    this->transferFamily = i;
                    supportedQueues |= VK_QUEUE_TRANSFER_BIT;
                }

                // Look for dedicated transfer queue (transfer only, no RenderSystem/compute)
                if (!dedicatedTransferFamily &&
                    queueFamily.queueFlags == VK_QUEUE_TRANSFER_BIT)
                {
                    dedicatedTransferFamily = i;
                }
            }
        }

        // Prefer dedicated queues for better async performance
        if (dedicatedComputeFamily)
        {
            this->computeFamily = *dedicatedComputeFamily;
            Log::Info("Using dedicated compute queue family\n");
        }

        if (dedicatedTransferFamily)
        {
            this->transferFamily = *dedicatedTransferFamily;
            Log::Info("Using dedicated transfer queue family\n");
        }

        if (!GraphicsFamily)
            throw std::runtime_error("Failed to find queue family supporting VK_QUEUE_GRAPHICS_BIT");

        if (!presentFamily)
            throw std::runtime_error("Failed to find queue family supporting present");

        if (!computeFamily)
            Log::Warning("No compute queue family found\n");

        if (!transferFamily)
            Log::Warning("No transfer queue family found\n");
    }

    void LogicalDevice::CreateLogicalDevice()
    {
        // Collect unique queue families
        std::unordered_set<uint32_t> uniqueQueueFamilies = {
            GraphicsFamily, presentFamily, computeFamily, transferFamily};

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        float queuePriority = 1.0f;

        for (uint32_t queueFamily : uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo = {};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        // Query available features using Vulkan 1.1+ feature chain
        VkPhysicalDeviceVulkan13Features vulkan13Features = {};
        vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

        VkPhysicalDeviceVulkan12Features vulkan12Features = {};
        vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        vulkan12Features.pNext = &vulkan13Features;

        VkPhysicalDeviceVulkan11Features vulkan11Features = {};
        vulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        vulkan11Features.pNext = &vulkan12Features;

        VkPhysicalDeviceFeatures2 deviceFeatures2 = {};
        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures2.pNext = &vulkan11Features;

        vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures2);

        auto &availableFeatures = deviceFeatures2.features;

        // Build requested features
        VkPhysicalDeviceFeatures requestedFeatures = {};

        // Core Vulkan 1.0 features
        if (availableFeatures.sampleRateShading)
            requestedFeatures.sampleRateShading = VK_TRUE;

        if (availableFeatures.fillModeNonSolid)
        {
            requestedFeatures.fillModeNonSolid = VK_TRUE;
            if (availableFeatures.wideLines)
                requestedFeatures.wideLines = VK_TRUE;
        }
        else
        {
            Log::Warning("Selected GPU does not support wireframe pipelines!\n");
        }

        if (availableFeatures.samplerAnisotropy)
            requestedFeatures.samplerAnisotropy = VK_TRUE;
        else
            Log::Warning("Selected GPU does not support sampler anisotropy!\n");

        // Texture compression
        if (availableFeatures.textureCompressionBC)
            requestedFeatures.textureCompressionBC = VK_TRUE;
        else if (availableFeatures.textureCompressionASTC_LDR)
            requestedFeatures.textureCompressionASTC_LDR = VK_TRUE;
        else if (availableFeatures.textureCompressionETC2)
            requestedFeatures.textureCompressionETC2 = VK_TRUE;

        if (availableFeatures.vertexPipelineStoresAndAtomics)
            requestedFeatures.vertexPipelineStoresAndAtomics = VK_TRUE;
        else
            Log::Warning("Selected GPU does not support vertex pipeline stores and atomics!\n");

        if (availableFeatures.fragmentStoresAndAtomics)
            requestedFeatures.fragmentStoresAndAtomics = VK_TRUE;
        else
            Log::Warning("Selected GPU does not support fragment stores and atomics!\n");

        if (availableFeatures.shaderStorageImageExtendedFormats)
            requestedFeatures.shaderStorageImageExtendedFormats = VK_TRUE;
        else
            Log::Warning("Selected GPU does not support shader storage extended formats!\n");

        if (availableFeatures.shaderStorageImageWriteWithoutFormat)
            requestedFeatures.shaderStorageImageWriteWithoutFormat = VK_TRUE;
        else
            Log::Warning("Selected GPU does not support shader storage write without format!\n");

        if (availableFeatures.geometryShader)
            requestedFeatures.geometryShader = VK_TRUE;
        else
            Log::Warning("Selected GPU does not support geometry shaders!\n");

        if (availableFeatures.tessellationShader)
            requestedFeatures.tessellationShader = VK_TRUE;
        else
            Log::Warning("Selected GPU does not support tessellation shaders!\n");

        if (availableFeatures.multiViewport)
            requestedFeatures.multiViewport = VK_TRUE;
        else
            Log::Warning("Selected GPU does not support multi viewports!\n");

        // Store enabled core features
        enabledFeatures = requestedFeatures;

        // Build Vulkan 1.1 feature requests
        VkPhysicalDeviceVulkan11Features requested11Features = {};
        requested11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;

        if (vulkan11Features.shaderDrawParameters)
            requested11Features.shaderDrawParameters = VK_TRUE;

        // Build Vulkan 1.2 feature requests
        VkPhysicalDeviceVulkan12Features requested12Features = {};
        requested12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        requested12Features.pNext = &requested11Features;

        // Timeline semaphores for better synchronization
        if (vulkan12Features.timelineSemaphore)
        {
            requested12Features.timelineSemaphore = VK_TRUE;
            Log::Info("Enabling timeline semaphores\n");
        }

        // Descriptor indexing for bindless rendering
        if (vulkan12Features.descriptorIndexing)
        {
            requested12Features.descriptorIndexing = VK_TRUE;
            requested12Features.shaderSampledImageArrayNonUniformIndexing =
                vulkan12Features.shaderSampledImageArrayNonUniformIndexing;
            requested12Features.runtimeDescriptorArray =
                vulkan12Features.runtimeDescriptorArray;
            requested12Features.descriptorBindingPartiallyBound =
                vulkan12Features.descriptorBindingPartiallyBound;
            requested12Features.descriptorBindingVariableDescriptorCount =
                vulkan12Features.descriptorBindingVariableDescriptorCount;
            Log::Info("Enabling descriptor indexing features\n");
        }

        // Buffer device address for advanced GPU-driven rendering
        if (vulkan12Features.bufferDeviceAddress)
        {
            requested12Features.bufferDeviceAddress = VK_TRUE;
            Log::Info("Enabling buffer device address\n");
        }

        // Scalar block layout for better shader data layout
        if (vulkan12Features.scalarBlockLayout)
        {
            requested12Features.scalarBlockLayout = VK_TRUE;
            Log::Info("Enabling scalar block layout\n");
        }

        // Host query reset
        if (vulkan12Features.hostQueryReset)
            requested12Features.hostQueryReset = VK_TRUE;

        // Build Vulkan 1.3 feature requests
        VkPhysicalDeviceVulkan13Features requested13Features = {};
        requested13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        requested13Features.pNext = &requested12Features;

        // Dynamic rendering (no more render passes!)
        if (vulkan13Features.dynamicRendering)
        {
            requested13Features.dynamicRendering = VK_TRUE;
            Log::Info("Enabling dynamic rendering\n");
        }

        // Synchronization2 for better sync primitives
        if (vulkan13Features.synchronization2)
        {
            requested13Features.synchronization2 = VK_TRUE;
            Log::Info("Enabling synchronization2\n");
        }

        // Enhanced maintenance features
        if (vulkan13Features.maintenance4)
            requested13Features.maintenance4 = VK_TRUE;

        // Create device with feature chain
        VkDeviceCreateInfo deviceCreateInfo = {};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pNext = &requested13Features; // Chain starts here
        deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();

        // Validation layers at device level are deprecated in Vulkan 1.1+
        // They only need to be enabled at instance level

        deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(DeviceExtensions.size());
        deviceCreateInfo.ppEnabledExtensionNames = DeviceExtensions.data();
        deviceCreateInfo.pEnabledFeatures = &requestedFeatures;

        RenderSystem::CheckVkResult(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &logicalDevice));

        volkLoadDevice(logicalDevice);

        // Retrieve queue handles
        vkGetDeviceQueue(logicalDevice, GraphicsFamily, 0, &GraphicsQueue);
        vkGetDeviceQueue(logicalDevice, presentFamily, 0, &presentQueue);
        vkGetDeviceQueue(logicalDevice, computeFamily, 0, &computeQueue);
        vkGetDeviceQueue(logicalDevice, transferFamily, 0, &transferQueue);
    }
}