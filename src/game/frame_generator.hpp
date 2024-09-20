#pragma once

#include "game/camera.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "gfx/vulkan/swapchain.hpp"
#include "transform.hpp"
#include <compare>
#include <functional>
#include <gfx/vulkan/image.hpp>
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
    class Game;

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
            std::optional<Transform>            transform;
            DynamicRenderingPass                render_pass;
            std::shared_ptr<vk::UniquePipeline> pipeline;
            std::array<vk::DescriptorSet, 4>    descriptors;
            std::function<void(vk::CommandBuffer, vk::PipelineLayout, U32)>
                record_func;

            std::strong_ordering
            operator<=> (const RecordObject& other) const noexcept;
        };
    public:

        explicit FrameGenerator(const game::Game*);
        ~FrameGenerator() = default;

        FrameGenerator(const FrameGenerator&)             = delete;
        FrameGenerator(FrameGenerator&&)                  = delete;
        FrameGenerator& operator= (const FrameGenerator&) = delete;
        FrameGenerator& operator= (FrameGenerator&&)      = delete;

        void generateFrame(Camera, std::span<const RecordObject>);

    private:
        void internalGenerateFrame(
            vk::CommandBuffer,
            U32 swapchainImageIdx,
            const gfx::vulkan::Swapchain&,
            std::span<const RecordObject>);

        const game::Game* game;
        bool              needs_resize_transitions;

        gfx::vulkan::Image2D depth_buffer;

        Camera camera;

        gfx::vulkan::Buffer mvp_matrices;
    };

} // namespace game
