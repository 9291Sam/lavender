#pragma once

#include "game/camera.hpp"
#include "gfx/profiler/profiler.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "gfx/vulkan/swapchain.hpp"
#include "transform.hpp"
#include <compare>
#include <functional>
#include <gfx/vulkan/image.hpp>
#include <imgui.h>
#include <memory>
#include <util/misc.hpp>
#include <vulkan/vulkan_format_traits.hpp>
#include <vulkan/vulkan_handles.hpp>

extern std::atomic<u32> numberOfFacesVisible;   // NOLINT
extern std::atomic<u32> numberOfFacesOnGpu;     // NOLINT
extern std::atomic<u32> numberOfFacesPossible;  // NOLINT
extern std::atomic<u32> numberOfFacesAllocated; // NOLINT

extern std::atomic<u32> numberOfChunksAllocated; // NOLINT
extern std::atomic<u32> numberOfChunksPossible;  // NOLINT

extern std::atomic<u32> numberOfBricksAllocated; // NOLINT
extern std::atomic<u32> numberOfBricksPossible;  // NOLINT

extern std::atomic<f32> flySpeed; // NOLINT

extern std::atomic<u32> f32TickDeltaTime; // NOLINT

namespace gfx
{
    class Renderer;

    namespace profiler
    {
        class ProfilerGraph;
    } // namespace profiler

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
        enum class DynamicRenderingPass : u8
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
            // Render imgui (unorm pass!)
            MenuRender                   = 5,
            // render everything else
            SimpleColor                  = 6,
            DynamicRenderingPassMaxValue = 7,
        };

        static std::string dynamicRenderingPassGetName(DynamicRenderingPass p)
        {
            switch (p)
            {
            case DynamicRenderingPass::PreFrameUpdate:
                return "PreFrameUpdate";
            case DynamicRenderingPass::VoxelRenderer:
                return "VoxelRenderer";
            case DynamicRenderingPass::VoxelVisibilityDetection:
                return "VoxelVisibilityDetection";
            case DynamicRenderingPass::VoxelColorCalculation:
                return "VoxelColorCalculation";
            case DynamicRenderingPass::VoxelColorTransfer:
                return "VoxelColorTransfer";
            case DynamicRenderingPass::MenuRender:
                return "MenuRender";
            case DynamicRenderingPass::SimpleColor:
                return "SimpleColor";
            case DynamicRenderingPass::DynamicRenderingPassMaxValue:
                return "DynamicRenderingPassMaxValue";
            }
        }

        struct RecordObject
        {
            std::optional<Transform>                                        transform;
            DynamicRenderingPass                                            render_pass;
            std::shared_ptr<vk::UniquePipeline>                             pipeline;
            std::array<vk::DescriptorSet, 4>                                descriptors;
            std::function<void(vk::CommandBuffer, vk::PipelineLayout, u32)> record_func;

            std::strong_ordering operator<=> (const RecordObject& other) const noexcept;
        };
    public:

        explicit FrameGenerator(const game::Game*);
        ~FrameGenerator();

        FrameGenerator(const FrameGenerator&)             = delete;
        FrameGenerator(FrameGenerator&&)                  = delete;
        FrameGenerator& operator= (const FrameGenerator&) = delete;
        FrameGenerator& operator= (FrameGenerator&&)      = delete;

        void generateFrame(
            Camera, std::span<const RecordObject>, std::span<const gfx::profiler::ProfilerTask>);

        // lives as long as the program
        vk::DescriptorSet getGlobalInfoDescriptorSet() const;
        std::shared_ptr<vk::UniqueDescriptorSetLayout>
        getGlobalInfoDescriptorSetLayout() const noexcept;

    private:

        struct GlobalFrameInfo
        {
            glm::mat4 model_matrix;
            glm::vec4 camera_position;
            u32       frame_number;
            float     time_alive;
        };

        struct GlobalInfoDescriptors
        {
            gfx::vulkan::GpuOnlyBuffer<glm::mat4>       mvp_matrices;
            gfx::vulkan::GpuOnlyBuffer<GlobalFrameInfo> global_frame_info;

            gfx::vulkan::Image2D depth_buffer;

            gfx::vulkan::Image2D visible_voxel_image;

            gfx::vulkan::Image2D menu_render_target;

            vk::UniqueSampler do_nothing_sampler;
        };

        void internalGenerateFrame(
            vk::CommandBuffer,
            u32 swapchainImageIdx,
            const gfx::vulkan::Swapchain&,
            std::size_t flyingFrameIdx,
            std::vector<RecordObject>,
            std::span<const gfx::profiler::ProfilerTask>);

        static GlobalInfoDescriptors makeGlobalDescriptors(const gfx::Renderer*, vk::DescriptorSet);

        const game::Game* game;
        bool              has_resize_ocurred;

        std::shared_ptr<vk::UniqueDescriptorSetLayout> set_layout;
        vk::DescriptorSet                              global_info_descriptor_set;

        Camera                                        camera;
        GlobalInfoDescriptors                         global_descriptors;
        ImFont*                                       font;
        std::unique_ptr<gfx::profiler::ProfilerGraph> tracing_graph;
        std::shared_ptr<vk::UniquePipeline>           menu_transfer_pipeline;
        std::shared_ptr<vk::UniquePipeline>           skybox_pipeline;
    };

} // namespace game
