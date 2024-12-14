// #include "flyer.hpp"
// #include "voxel/constants.hpp"
// #include "voxel/voxel.hpp"
// #include "voxel/world.hpp"

// namespace verdigris
// {

//     Flyer::Flyer( // NOLINTNEXTLINE
//         glm::vec3    start_,
//         glm::vec3    dir_,
//         f32          speed_,
//         voxel::Voxel material_)
//         : start {start_}
//         , dir {dir_}
//         , previous_position {this->start}
//         , current_position {this->previous_position}
//         , speed {speed_}
//         , material {material_}
//         , time_alive {0.0f}
//     {}

//     void Flyer::update(f32 deltaTime)
//     {
//         if (this->time_alive < 10.0f)
//         {
//             this->previous_position = this->current_position;
//             this->current_position  = {this->start + this->time_alive * this->dir * speed};
//         }

//         this->time_alive += deltaTime;
//     }

//     void Flyer::display(voxel::World& w)
//     {
//         if (this->time_alive > 10.0f)
//         {
//             if (this->previous_position != voxel::WorldPosition {})
//             {
//                 w.writeVoxel(this->previous_position, voxel::Voxel::NullAirEmpty);
//                 this->previous_position = voxel::WorldPosition {};
//             }

//             return;
//         }

//         if (this->current_position != this->previous_position)
//         {
//             // std::array<voxel::World::VoxelWrite, 2> writes {
//             //     voxel::World::VoxelWrite {
//             //         .position {this->previous_position}, .voxel {voxel::Voxel::NullAirEmpty}},
//             //     voxel::World::VoxelWrite {
//             //         .position {this->current_position}, .voxel {this->material}},
//             // };

//             // w.writeVoxel(std::vector<voxel::World::VoxelWrite> {writes.cbegin(),
//             writes.cend()});
//         }
//     }
// } // namespace verdigris