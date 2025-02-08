#include "device.hpp"
#include "util/log.hpp"
#include "util/misc.hpp"
#include "util/threads.hpp"
#include <algorithm>
#include <optional>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_hpp_macros.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <vulkan/vulkan_to_string.hpp>

namespace gfx::vulkan
{
    Device::Device(vk::Instance instance, vk::SurfaceKHR surface) // NOLINT
    {
        const std::vector<vk::PhysicalDevice> availablePhysicalDevices =
            instance.enumeratePhysicalDevices();

        const auto getRatingOfDevice = [](vk::PhysicalDevice d) -> std::size_t
        {
            std::size_t score = 0;

            const vk::PhysicalDeviceProperties properties = d.getProperties();

            score += properties.limits.maxImageDimension2D;
            score += properties.limits.maxImageDimension3D;

            return score;
        };

        this->physical_device = *std::max_element(
            availablePhysicalDevices.cbegin(),
            availablePhysicalDevices.cend(),
            [&](vk::PhysicalDevice l, vk::PhysicalDevice r)
            {
                return getRatingOfDevice(l) < getRatingOfDevice(r);
            });

        this->physical_device_type = this->physical_device.getProperties().deviceType;

        this->is_physical_device_amd = this->physical_device.getProperties().vendorID == 0x1002;

        const std::vector<vk::QueueFamilyProperties> queueFamilyProperties =
            this->physical_device.getQueueFamilyProperties();

        std::optional<u32> graphicsFamily      = std::nullopt;
        std::optional<u32> asyncComputeFamily  = std::nullopt;
        std::optional<u32> asyncTransferFamily = std::nullopt;

        for (u32 i = 0; i < queueFamilyProperties.size(); ++i)
        {
            const vk::QueueFlags flags = queueFamilyProperties[i].queueFlags;

            if (flags & vk::QueueFlagBits::eVideoDecodeKHR
                || flags & vk::QueueFlagBits::eVideoEncodeKHR
                || flags & vk::QueueFlagBits::eOpticalFlowNV)
            {
                continue;
            }

            if (flags & vk::QueueFlagBits::eGraphics && graphicsFamily == std::nullopt)
            {
                graphicsFamily = i;

                const vk::Bool32 isSurfaceSupported =
                    this->physical_device.getSurfaceSupportKHR(i, surface);

                util::assertFatal(
                    flags & vk::QueueFlagBits::eCompute && flags & vk::QueueFlagBits::eTransfer
                        && static_cast<bool>(isSurfaceSupported),
                    "Tried to instantiate a graphics queue with flags {}, {}",
                    vk::to_string(flags),
                    isSurfaceSupported);

                continue;
            }

            if (flags & vk::QueueFlagBits::eCompute && asyncComputeFamily == std::nullopt)
            {
                asyncComputeFamily = i;

                util::assertFatal(
                    static_cast<bool>(flags & vk::QueueFlagBits::eTransfer),
                    "Tried to instantiate a compute queue with flags {}",
                    vk::to_string(flags));

                continue;
            }

            if (flags & vk::QueueFlagBits::eTransfer && asyncTransferFamily == std::nullopt)
            {
                asyncTransferFamily = i;

                continue;
            }
        }

        this->queue_family_indexes =
            std::array {graphicsFamily, asyncComputeFamily, asyncTransferFamily};

        auto getStringOfFamily = [](std::optional<u32> f) -> std::string
        {
            if (f.has_value()) // NOLINT
            {
                return std::to_string(*f);
            }
            else
            {
                return "std::nullopt";
            }
        };

        util::logLog(
            "Discovered queues! | Graphics: {} | Async Compute: {} | Async "
            "Transfer: {}",
            getStringOfFamily(graphicsFamily),
            getStringOfFamily(asyncComputeFamily),
            getStringOfFamily(asyncTransferFamily));

        std::vector<vk::DeviceQueueCreateInfo> queuesToCreate {};

        std::vector<f32> queuePriorities {};
        queuePriorities.resize(1024, 1.0);

        u32 numberOfGraphicsQueues      = 0;
        u32 numberOfAsyncComputeQueues  = 0;
        u32 numberOfAsyncTransferQueues = 0;

        if (graphicsFamily.has_value())
        {
            numberOfGraphicsQueues = queueFamilyProperties.at(*graphicsFamily).queueCount;

            queuesToCreate.push_back(vk::DeviceQueueCreateInfo {
                .sType {vk::StructureType::eDeviceQueueCreateInfo},
                .pNext {nullptr},
                .flags {},
                .queueFamilyIndex {*graphicsFamily},
                .queueCount {numberOfGraphicsQueues},
                .pQueuePriorities {queuePriorities.data()},
            });
        }
        else
        {
            util::panic("No graphics queue available!");
        }

        if (asyncComputeFamily.has_value())
        {
            numberOfAsyncComputeQueues = queueFamilyProperties.at(*asyncComputeFamily).queueCount;

            queuesToCreate.push_back(vk::DeviceQueueCreateInfo {
                .sType {vk::StructureType::eDeviceQueueCreateInfo},
                .pNext {nullptr},
                .flags {},
                .queueFamilyIndex {*asyncComputeFamily},
                .queueCount {numberOfAsyncComputeQueues},
                .pQueuePriorities {queuePriorities.data()},
            });
        }

        if (asyncTransferFamily.has_value())
        {
            numberOfAsyncTransferQueues = queueFamilyProperties.at(*asyncTransferFamily).queueCount;

            queuesToCreate.push_back(vk::DeviceQueueCreateInfo {
                .sType {vk::StructureType::eDeviceQueueCreateInfo},
                .pNext {nullptr},
                .flags {},
                .queueFamilyIndex {*asyncTransferFamily},
                .queueCount {numberOfAsyncTransferQueues},
                .pQueuePriorities {queuePriorities.data()},
            });
        }

        util::logLog(
            "Instantiating queues! | Graphics: {} | Async Compute: {} | Async "
            "Transfer: {}",
            numberOfGraphicsQueues,
            numberOfAsyncComputeQueues,
            numberOfAsyncTransferQueues);

        this->queue_family_numbers = std::array {
            numberOfGraphicsQueues, numberOfAsyncComputeQueues, numberOfAsyncTransferQueues};

        std::array requiredExtensions {
            vk::KHRDynamicRenderingExtensionName,
            vk::KHRSwapchainExtensionName,
#ifdef __APPLE__
            "VK_KHR_portability_subset",
#endif // __APPLE__,
        };

        vk::PhysicalDeviceFeatures features {};
        features.shaderInt16              = vk::True;
        features.multiDrawIndirect        = vk::True;
        features.fragmentStoresAndAtomics = vk::True;

        vk::PhysicalDeviceVulkan12Features features12 {};
        features12.sType                  = vk::StructureType::ePhysicalDeviceVulkan12Features;
        features12.pNext                  = nullptr;
        features12.runtimeDescriptorArray = vk::True;

        vk::PhysicalDeviceVulkan11Features features11 {};
        features11.sType                    = vk::StructureType::ePhysicalDeviceVulkan11Features;
        features11.pNext                    = &features12;
        features11.storageBuffer16BitAccess = vk::True;

        const vk::PhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures {
            .sType {vk::StructureType::ePhysicalDeviceDynamicRenderingFeatures},
            .pNext {&features11},
            .dynamicRendering {vk::True}};

        const vk::DeviceCreateInfo deviceCreateInfo {
            .sType {vk::StructureType::eDeviceCreateInfo},
            .pNext {&dynamicRenderingFeatures},
            .flags {},
            .queueCreateInfoCount {static_cast<u32>(queuesToCreate.size())},
            .pQueueCreateInfos {queuesToCreate.data()},
            .enabledLayerCount {0},
            .ppEnabledLayerNames {nullptr},
            .enabledExtensionCount {static_cast<u32>(requiredExtensions.size())},
            .ppEnabledExtensionNames {requiredExtensions.data()},
            .pEnabledFeatures {&features},
        };

        this->device = this->physical_device.createDeviceUnique(deviceCreateInfo);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(instance, *this->device);

        if constexpr (util::isDebugBuild())
        {
            this->device->setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {
                .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                .pNext {nullptr},
                .objectType {vk::ObjectType::eDevice},
                .objectHandle {std::bit_cast<u64>(*this->device)},
                .pObjectName {"Device"},
            });
        }

        std::vector<util::Mutex<vk::Queue>> graphicsQueues {};
        std::vector<util::Mutex<vk::Queue>> asyncComputeQueues {};
        std::vector<util::Mutex<vk::Queue>> asyncTransferQueues {};

        for (u32 idx = 0; idx < numberOfGraphicsQueues; ++idx)
        {
            vk::Queue q = this->device->getQueue(*graphicsFamily, idx);

            if constexpr (util::isDebugBuild())
            {
                std::string name = std::format("Graphics Queue #{}", idx);

                device->setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {
                    .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                    .pNext {nullptr},
                    .objectType {vk::ObjectType::eQueue},
                    .objectHandle {std::bit_cast<u64>(q)},
                    .pObjectName {name.c_str()},
                });
            }

            graphicsQueues.push_back(util::Mutex {std::move(q)}); // NOLINT
        }

        for (u32 idx = 0; idx < numberOfAsyncComputeQueues; ++idx)
        {
            vk::Queue q = this->device->getQueue(*asyncComputeFamily, idx);

            if constexpr (util::isDebugBuild())
            {
                std::string name = std::format("Async Compute Queue #{}", idx);

                device->setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {
                    .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                    .pNext {nullptr},
                    .objectType {vk::ObjectType::eQueue},
                    .objectHandle {std::bit_cast<u64>(q)},
                    .pObjectName {name.c_str()},
                });
            }

            asyncComputeQueues.push_back(util::Mutex {std::move(q)}); // NOLINT
        }

        for (u32 idx = 0; idx < numberOfAsyncTransferQueues; ++idx)
        {
            vk::Queue q = this->device->getQueue(*asyncTransferFamily, idx);

            if constexpr (util::isDebugBuild())
            {
                std::string name = std::format("Async Transfer Queue #{}", idx);

                device->setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT {
                    .sType {vk::StructureType::eDebugUtilsObjectNameInfoEXT},
                    .pNext {nullptr},
                    .objectType {vk::ObjectType::eQueue},
                    .objectHandle {std::bit_cast<u64>(q)},
                    .pObjectName {name.c_str()},
                });
            }

            asyncTransferQueues.push_back(util::Mutex {std::move(q)}); // NOLINT
        }

        this->queues[static_cast<std::size_t>(QueueType::Graphics)] = std::move(graphicsQueues);
        this->queues[static_cast<std::size_t>(QueueType::AsyncCompute)] =
            std::move(asyncComputeQueues);
        this->queues[static_cast<std::size_t>(QueueType::AsyncTransfer)] =
            std::move(asyncTransferQueues);

        util::logTrace("Created device");
    }

    std::optional<u32> Device::getFamilyOfQueueType(QueueType t) const noexcept
    {
        return this->queue_family_indexes.at(static_cast<std::size_t>(util::toUnderlying(t)));
    }

    u32 Device::getNumberOfQueues(QueueType t) const noexcept
    {
        return this->queue_family_numbers.at(static_cast<std::size_t>(util::toUnderlying(t)));
    }

    bool Device::isIntegrated() const noexcept
    {
        return this->physical_device_type == vk::PhysicalDeviceType::eIntegratedGpu;
    }

    vk::PhysicalDevice Device::getPhysicalDevice() const noexcept
    {
        return this->physical_device;
    }

    bool Device::isAmd() const noexcept
    {
        return this->is_physical_device_amd;
    }

    vk::Device Device::getDevice() const noexcept
    {
        return *this->device;
    }
} // namespace gfx::vulkan
