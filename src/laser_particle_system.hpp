#pragma once

#include "game/camera.hpp"
#include "game/frame_generator.hpp"
#include "game/game.hpp"
#include "gfx/renderer.hpp"
#include "gfx/vulkan/buffer.hpp"
#include "util/opaque_integer_handle.hpp"
#include "util/range_allocator.hpp"
#include <vulkan/vulkan_handles.hpp>
namespace render
{
    class LaserParticleSystem
    {
    public:
        using LaserHandle = util::OpaqueHandle<"render::LaserParticleSystem::LaserHandle", u8>;

        struct ParticleSystemConfiguration
        {
            glm::vec3 start;
            glm::vec3 end;
            f32       density;
            f32       spread;
            f32       scale;
            glm::vec4 color;
        };
    public:
        explicit LaserParticleSystem(const gfx::Renderer*);
        ~LaserParticleSystem();

        LaserParticleSystem(const LaserParticleSystem&)             = delete;
        LaserParticleSystem(LaserParticleSystem&&)                  = delete;
        LaserParticleSystem& operator= (const LaserParticleSystem&) = delete;
        LaserParticleSystem& operator= (LaserParticleSystem&&)      = delete;

        [[nodiscard]] LaserHandle createParticleSystem(ParticleSystemConfiguration);
        void                      destroyParticleSystem(LaserHandle);

        game::FrameGenerator::RecordObject getRecordObject(const game::Camera&, const game::Game&);

    private:

        const gfx::Renderer* renderer;

        util::OpaqueHandleAllocator<LaserHandle> handle_allocator;
        std::array<
            std::optional<util::RangeAllocation>,
            std::numeric_limits<LaserHandle::IndexType>::max()>
            configs;

        struct LaserParticleInfo
        {
            f32 position_x;
            f32 position_y;
            f32 position_z;
            f32 scale;
            u32 color;
        };

        util::RangeAllocator                                  buffer_position_allocator;
        gfx::vulkan::WriteOnlyBuffer<LaserParticleInfo>       particles_buffer;
        gfx::vulkan::WriteOnlyBuffer<vk::DrawIndirectCommand> draw_indirect_commands;

        std::shared_ptr<vk::UniqueDescriptorSetLayout> particle_render_descriptor_set_layout;
        vk::DescriptorSet                              particle_render_descriptor_set;

        std::shared_ptr<vk::UniquePipeline> particle_render_pipeline;
    };
} // namespace render