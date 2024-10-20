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
            // Outside of renderpass, put your transfer operations here
            PreFrameUpdate               = 0,
            // Rendering of Voxels to u32 image
            VoxelRenderer                = 1,
            // detect which voxels are visible and write id to new u32 image
            VoxelVisibilityDetection     = 2,
            // calculate the colors / shadows / GI etc...
            VoxelColorCalculation        = 3,
            // write voxel colors back to the color buffer
            VoxelColorTransfer           = 4,
            // render everything else
            SimpleColor                  = 5,
            DynamicRenderingPassMaxValue = 6,
        };

        struct RecordObject
        {
            std::optional<Transform>            transform;
            DynamicRenderingPass                render_pass;
            std::shared_ptr<vk::UniquePipeline> pipeline;
            std::array<vk::DescriptorSet, 4>    descriptors;
            std::function<void(vk::CommandBuffer, vk::PipelineLayout, u32)>
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

        // lives as long as the program
        vk::DescriptorSet getGlobalInfoDescriptorSet() const;
        std::shared_ptr<vk::UniqueDescriptorSetLayout>
        getGlobalInfoDescriptorSetLayout() const noexcept;

    private:

        struct GlobalFrameInfo
        {
            glm::vec4 camera_position;
            u32       frame_number;
            float     time_alive;
        };

        struct GlobalInfoDescriptors
        {
            gfx::vulkan::Buffer<glm::mat4>       mvp_matrices;
            gfx::vulkan::Buffer<GlobalFrameInfo> global_frame_info;

            gfx::vulkan::Image2D depth_buffer;

            gfx::vulkan::Image2D visible_voxel_image;

            vk::UniqueSampler do_nothing_sampler;
        };

        void internalGenerateFrame(
            vk::CommandBuffer,
            u32 swapchainImageIdx,
            const gfx::vulkan::Swapchain&,
            std::size_t flyingFrameIdx,
            std::span<const RecordObject>);

        static GlobalInfoDescriptors
        makeGlobalDescriptors(const gfx::Renderer*, vk::DescriptorSet);

        const game::Game* game;
        bool              has_resize_ocurred;

        std::shared_ptr<vk::UniqueDescriptorSetLayout> set_layout;
        vk::DescriptorSet global_info_descriptor_set;

        Camera                camera;
        GlobalInfoDescriptors global_descriptors;
    };

} // namespace game
