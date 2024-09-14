#pragma once

#include "gfx/vulkan/swapchain.hpp"
#include <compare>
#include <functional>
#include <memory>
#include <util/misc.hpp>
#include <vulkan/vulkan_format_traits.hpp>
#include <vulkan/vulkan_handles.hpp>

namespace gfx
{
    class Renderer;

    namespace vulkan
    {
        class Swapchain;
    } // namespace vulkan
} // namespace gfx

namespace game
{
    class FrameGenerator
    {
    public:
        // TODO: there's better ways to write this, but this works for now
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
            // TODO: replace with a pointer to member
            std::function<void(vk::CommandBuffer)> record_func;

            std::strong_ordering
            operator<=> (const RecordObject& other) const noexcept;
        };
    public:

        explicit FrameGenerator(const gfx::Renderer*);
        ~FrameGenerator() = default;

        FrameGenerator(const FrameGenerator&)             = delete;
        FrameGenerator(FrameGenerator&&)                  = delete;
        FrameGenerator& operator= (const FrameGenerator&) = delete;
        FrameGenerator& operator= (FrameGenerator&&)      = delete;

        void generateFrame(std::span<RecordObject>);

    private:
        void internalGenerateFrame(
            vk::CommandBuffer,
            U32 swapchainImageIdx,
            const gfx::vulkan::Swapchain&,
            std::span<const RecordObject>);

        const gfx::Renderer* renderer;
        bool                 needs_resize_transitions;
        // TODO: depth buffer
    };

} // namespace game
