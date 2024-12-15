#include "instance.hpp"
#include "allocator.hpp"
#include "util/misc.hpp"
#include <gfx/window.hpp>
#include <util/log.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_hpp_macros.hpp>

// NOLINTBEGIN clang-format off

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#define VMA_IMPLEMENTATION           1
#define VMA_STATIC_VULKAN_FUNCTIONS  0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>
#undef VMA_IMPLEMENTATION
#undef VMA_STATIC_VULKAN_FUNCTIONS
#undef VMA_DYNAMIC_VULKAN_FUNCTIONS

#pragma clang diagnostic pop
// NOLINTEND clang-format on

namespace gfx::vulkan
{
    Instance::Instance()
        : vulkan_api_version(vk::ApiVersion13)
    {
        util::assertFatal(
            this->vulkan_loader.success(),
            "Failed to load Vulkan, is it supported on your system?");

        vk::defaultDispatchLoaderDynamic.init(this->vulkan_loader);

        const vk::ApplicationInfo applicationInfo {
            .sType {vk::StructureType::eApplicationInfo},
            .pNext {nullptr},
            .pApplicationName {"lavender"},
            .applicationVersion {vk::makeApiVersion(
                LAVENDER_VERSION_TWEAK,
                LAVENDER_VERSION_MAJOR,
                LAVENDER_VERSION_MINOR,
                LAVENDER_VERSION_PATCH)},
            .pEngineName {"lavender"},
            .engineVersion {vk::makeApiVersion(
                LAVENDER_VERSION_TWEAK,
                LAVENDER_VERSION_MAJOR,
                LAVENDER_VERSION_MINOR,
                LAVENDER_VERSION_PATCH)},
            .apiVersion {this->vulkan_api_version},
        };

        const std::vector<vk::LayerProperties> availableLayers =
            vk::enumerateInstanceLayerProperties();

        std::vector<const char*> layers {};
        if constexpr (util::isDebugBuild())
        {
            layers.push_back("VK_LAYER_KHRONOS_validation");
        }

        for (const char* requestedLayer : layers)
        {
            for (const vk::LayerProperties& availableLayer : availableLayers)
            {
                if (std::strcmp(availableLayer.layerName.data(), requestedLayer) == 0)
                {
                    goto next_layer;
                }
            }

            util::assertFatal(false, "Required layer {} was not available!", requestedLayer);
        next_layer: {}
        }

        std::vector<const char*> extensions {};

#ifdef __APPLE__
        extensions.push_back(vk::KHRPortabilityEnumerationExtensionName);
#endif // __APPLE__
        for (const char* e : Window::getRequiredExtensions())
        {
            extensions.push_back(e);
        }

        if constexpr (util::isDebugBuild())
        {
            extensions.push_back(vk::EXTDebugUtilsExtensionName);
        }

        const std::vector<vk::ExtensionProperties> availableExtensions =
            vk::enumerateInstanceExtensionProperties();

        for (const char* requestedExtension : extensions)
        {
            for (const vk::ExtensionProperties& availableExtension : availableExtensions)
            {
                if (std::strcmp(availableExtension.extensionName.data(), requestedExtension) == 0)
                {
                    goto next_extension;
                }
            }

            util::assertFatal(
                false, "Required extension {} was not available!", requestedExtension);
        next_extension: {}

            // util::logTrace(
            //     "Requesting instance extension {}", requestedExtension);
        }

        static auto debugMessengerCallback =
            [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
               VkDebugUtilsMessageTypeFlagsEXT /*messageType*/,
               const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
               [[maybe_unused]] void*                      pUserData) -> vk::Bool32
        {
            switch (static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity))
            {
            case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
                util::logTrace("{}", pCallbackData->pMessage);
                break;
            case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
                util::logLog("{}", pCallbackData->pMessage);
                break;
            case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
                util::logWarn("{}", pCallbackData->pMessage);
                break;
            case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
                util::logFatal("{}", pCallbackData->pMessage);
                break;
            default:
                break;
            }

            std::string msg = pCallbackData->pMessage;

            if (msg.contains("ObjectName"))
            {
                util::debugBreak();
            }
            return vk::False;
        };

        const vk::DebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo {
            .sType {vk::StructureType::eDebugUtilsMessengerCreateInfoEXT},
            .pNext {nullptr},
            .flags {},
            .messageSeverity {
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
                | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning},
            .messageType {
                vk::DebugUtilsMessageTypeFlagBitsEXT::eDeviceAddressBinding
                | vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
                | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation},
            .pfnUserCallback {debugMessengerCallback},
            .pUserData {nullptr},
        };

        const vk::InstanceCreateInfo instanceCreateInfo {
            .sType {vk::StructureType::eInstanceCreateInfo},
            .pNext {util::isDebugBuild() ? &debugMessengerCreateInfo : nullptr},
            .flags {
#ifdef __APPLE__
                vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR
#endif // __APPLE__
            },
            .pApplicationInfo {&applicationInfo},
            .enabledLayerCount {static_cast<u32>(layers.size())},
            .ppEnabledLayerNames {layers.data()},
            .enabledExtensionCount {static_cast<uint32_t>(extensions.size())},
            .ppEnabledExtensionNames {extensions.data()}};

        this->instance = vk::createInstanceUnique(instanceCreateInfo);

        vk::defaultDispatchLoaderDynamic.init(*this->instance);

        if constexpr (util::isDebugBuild())
        {
            this->debug_messenger =
                this->instance->createDebugUtilsMessengerEXTUnique(debugMessengerCreateInfo);
        }
    }

    vk::Instance Instance::operator* () const noexcept
    {
        return *this->instance;
    }

    u32 Instance::getVulkanVersion() const noexcept
    {
        return this->vulkan_api_version;
    }

    const vk::DynamicLoader& Instance::getLoader() const noexcept
    {
        return this->vulkan_loader;
    }
} // namespace gfx::vulkan
