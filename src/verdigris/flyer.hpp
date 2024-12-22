#pragma once

#include "voxel/structures.hpp"
#include "voxel/world_manager.hpp"
#include <glm/gtx/hash.hpp>

namespace verdigris
{

    class Flyer
    {
    public:

        Flyer(
            glm::vec3 start,
            glm::vec3 dir,
            f32       speed,
            voxel::WorldManager*,
            f32          maxTime  = 10.0f,
            voxel::Voxel material = voxel::Voxel::Emerald);
        ~Flyer() = default;

        Flyer(const Flyer&)             = delete;
        Flyer(Flyer&&)                  = default;
        Flyer& operator= (const Flyer&) = delete;
        Flyer& operator= (Flyer&&)      = default;

        void update(f32 deltaTime);

    private:
        voxel::WorldManager*             world_manager;
        glm::vec3                        start;
        glm::vec3                        dir;
        voxel::WorldPosition             current_position;
        f32                              speed;
        f32                              max_time_alive;
        f32                              time_alive;
        voxel::WorldManager::VoxelObject voxel_object;
    };
} // namespace verdigris
