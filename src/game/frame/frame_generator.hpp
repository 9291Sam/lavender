#pragma once

#include <compare>
#include <functional>
#include <memory>
#include <util/misc.hpp>
#include <vulkan/vulkan_format_traits.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace gfx::vulkan
{
    class Swapchain;
    class Device;
} // namespace gfx::vulkan

namespace game::frame
{
    // TODO: theres better ways to write this, but this works for now
    enum class DynamicRenderingPass
    {
        SimpleColor                  = 0,
        DynamicRenderingPassMaxValue = 1,
    };

    struct RecordObject
    {
        DynamicRenderingPass                   render_pass;
        std::shared_ptr<vk::UniquePipeline>    pipeline;
        std::array<vk::DescriptorSet, 4>       descriptors;
        std::function<void(vk::CommandBuffer)> record_func;

        std::strong_ordering
        operator<=> (const RecordObject& other) const noexcept;
    };

    void generateFrame(
        vk::CommandBuffer,
        U32 swapchainImageIdx,
        const gfx::vulkan::Swapchain&,
        const gfx::vulkan::Device&,
        std::span<RecordObject>);

} // namespace game::frame
