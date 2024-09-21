#pragma once

#include "util/misc.hpp"
#include <vulkan/vulkan_format_traits.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace gfx::vulkan
{
    class Instance
    {
    public:
        Instance();
        ~Instance() = default;

        Instance(const Instance&)             = delete;
        Instance(Instance&&)                  = delete;
        Instance& operator= (const Instance&) = delete;
        Instance& operator= (Instance&&)      = delete;

        vk::Instance      operator* () const noexcept;
        [[nodiscard]] u32 getVulkanVersion() const noexcept;

    private:
        vk::DynamicLoader                vulkan_loader;
        vk::UniqueInstance               instance;
        vk::UniqueDebugUtilsMessengerEXT debug_messenger;
        u32                              vulkan_api_version;
    };
} // namespace gfx::vulkan
