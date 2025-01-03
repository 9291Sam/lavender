// #include "flyer.hpp"
// #include "voxel/structures.hpp"

// namespace verdigris
// {

//     Flyer::Flyer( // NOLINTNEXTLINE
//         glm::vec3            start_,
//         glm::vec3            dir_,
//         f32                  speed_,
//         voxel::WorldManager* worldManager,
//         f32                  maxTime,
//         voxel::Voxel         material)
//         : world_manager {worldManager}
//         , start {start_}
//         , dir {dir_}
//         , current_position {this->start}
//         , speed {speed_}
//         , max_time_alive {maxTime}
//         , time_alive {0.0f}
//     {
//         voxel::MaterialBrick brick {};

//         brick.modify(voxel::BrickLocalPosition {{0, 0, 0}}) = material;

//         this->voxel_object = this->world_manager->createVoxelObject(
//             voxel::LinearVoxelVolume {brick}, this->current_position);
//     }

//     Flyer::~Flyer()
//     {
//         this->free();
//     }

//     void Flyer::update(f32 deltaTime)
//     {
//         if (this->time_alive < 10.0f)
//         {
//             this->current_position =
//                 voxel::WorldPosition {this->start + this->time_alive * this->dir * speed};
//             this->world_manager->setVoxelObjectPosition(this->voxel_object,
//             this->current_position);
//         }
//         else
//         {
//             this->free();
//         }

//         this->time_alive += deltaTime;
//     }

//     void Flyer::free()
//     {
//         if (!this->voxel_object.isNull())
//         {
//             this->world_manager->destroyVoxelObject(std::move(this->voxel_object));
//         }
//     }
// } // namespace verdigris