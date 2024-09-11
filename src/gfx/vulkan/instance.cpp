#include "instance.hpp"
#include "util/misc.hpp"
#include <gfx/window.hpp>
#include <util/log.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_hpp_macros.hpp>

// NOLINTNEXTLINE
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace gfx::vulkan
{
    Instance::Instance()
        : vulkan_api_version(vk::ApiVersion12)
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

        for (const vk::LayerProperties& l : availableLayers)
        {
            util::logTrace("Layer {} is available", l.layerName.data());
        }

        const std::array layers {
            "VK_LAYER_KHRONOS_validation",
            "VK_LAYER_KHRONOS_synchronization2",
            "VK_LAYER_KHRONOS_shader_object"};

        for (const char* requestedLayer : layers)
        {
            for (const vk::LayerProperties& availableLayer : availableLayers)
            {
                if (std::strcmp(availableLayer.layerName.data(), requestedLayer)
                    == 0)
                {
                    goto next_layer;
                }
            }

            util::assertFatal(
                false, "Required layer {} was not available!", requestedLayer);
        next_layer:
            util::logTrace("Requesting layer {}", requestedLayer);
        }

        std::vector<const char*> extensions {};

        extensions.push_back(vk::KHRPortabilityEnumerationExtensionName);
        extensions.append_range(Window::getRequiredExtensions());

        if constexpr (util::isDebugBuild())
        {
            extensions.push_back(vk::EXTDebugUtilsExtensionName);
            extensions.push_back(vk::EXTLayerSettingsExtensionName);
        }

        const std::vector<vk::ExtensionProperties> availableExtensions =
            vk::enumerateInstanceExtensionProperties();

        for (const vk::ExtensionProperties& availableExtension :
             availableExtensions)
        {
            util::logTrace(
                "Instance extension {} is available",
                availableExtension.extensionName.data());
        }

        for (const char* requestedExtension : extensions)
        {
            for (const vk::ExtensionProperties& availableExtension :
                 availableExtensions)
            {
                if (std::strcmp(
                        availableExtension.extensionName.data(),
                        requestedExtension)
                    == 0)
                {
                    goto next_extension;
                }
            }

            util::assertFatal(
                false,
                "Required extension {} was not available!",
                requestedExtension);
        next_extension:

            util::logTrace(
                "Requesting instance extension {}", requestedExtension);
        }

        static auto debugMessengerCallback =
            [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
               VkDebugUtilsMessageTypeFlagsEXT /*messageType*/,
               const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
               [[maybe_unused]] void* pUserData) -> vk::Bool32
        {
            switch (static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(
                messageSeverity))
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
            }
            // util::panic("kill");

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
            .flags {vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR},
            .pApplicationInfo {&applicationInfo},
            .enabledLayerCount {layers.size()},
            .ppEnabledLayerNames {layers.data()},
            .enabledExtensionCount {static_cast<uint32_t>(extensions.size())},
            .ppEnabledExtensionNames {extensions.data()}};

        this->instance = vk::createInstanceUnique(instanceCreateInfo);

        vk::defaultDispatchLoaderDynamic.init(*this->instance);

        if constexpr (util::isDebugBuild())
        {
            this->debug_messenger =
                this->instance->createDebugUtilsMessengerEXTUnique(
                    debugMessengerCreateInfo);
        }
    }

    vk::Instance Instance::operator* () const noexcept
    {
        return *this->instance;
    }

    U32 Instance::getVulkanVersion() const noexcept
    {
        return this->vulkan_api_version;
    }
} // namespace gfx::vulkan