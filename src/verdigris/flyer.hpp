// #pragma once

// #include "voxel/constants.hpp"
// #include "voxel/voxel.hpp"
// #include "voxel/world.hpp"
// #include <glm/gtx/hash.hpp>

// namespace verdigris
// {

//     class Flyer
//     {
//     public:

//         Flyer(
//             glm::vec3    start,
//             glm::vec3    dir,
//             f32          speed,
//             voxel::Voxel material = voxel::Voxel::Emerald);
//         ~Flyer() = default;

//         Flyer(const Flyer&)             = delete;
//         Flyer(Flyer&&)                  = default;
//         Flyer& operator= (const Flyer&) = delete;
//         Flyer& operator= (Flyer&&)      = default;

//         void update(f32);
//         void display(voxel::World&);

//     private:
//         glm::vec3            start;
//         glm::vec3            dir;
//         voxel::WorldPosition previous_position;
//         voxel::WorldPosition current_position;
//         f32                  speed;
//         voxel::Voxel         material;
//         f32                  time_alive;
//     };
// } // namespace verdigris
