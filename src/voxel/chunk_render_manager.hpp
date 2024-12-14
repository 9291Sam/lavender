// #pragma once

// #include "util/misc.hpp"
// #include "util/opaque_integer_handle.hpp"
// #include "voxel/constants.hpp"
// #include "voxel/point_light.hpp"
// #include "voxel/voxel.hpp"
// #include <span>

// namespace game
// {
//     class Game;
// } // namespace game

// namespace old_voxel
// {
//     class ChunkRenderManager
//     {
//     public:

//         using Chunk =
//             util::OpaqueHandle<"voxel::ChunkRenderManager::Chunk", u16, ChunkRenderManager>;
//         using PointLight =
//             util::OpaqueHandle<"voxel::ChunkRenderManager::PointLight", u8, ChunkRenderManager>;

//         class ChunkLocalUpdate
//         {
//         public:
//             enum class ShadowUpdate : u8
//             {
//                 ShadowTransparent = 0,
//                 ShadowCasting     = 1,
//             };

//             enum class CameraVisibleUpdate : u8
//             {
//                 CameraInvisible = 0,
//                 CameraVisible   = 1,
//             };
//         public:
//             ChunkLocalUpdate(ChunkLocalPosition, Voxel, ShadowUpdate, CameraVisibleUpdate);
//             ~ChunkLocalUpdate() = default;

//             ChunkLocalUpdate(const ChunkLocalUpdate&)             = default;
//             ChunkLocalUpdate(ChunkLocalUpdate&&)                  = default;
//             ChunkLocalUpdate& operator= (const ChunkLocalUpdate&) = default;
//             ChunkLocalUpdate& operator= (ChunkLocalUpdate&&)      = default;

//             [[nodiscard]] ChunkLocalPosition  getPosition() const noexcept;
//             [[nodiscard]] Voxel               getVoxel() const noexcept;
//             [[nodiscard]] ShadowUpdate        getShadowUpdate() const noexcept;
//             [[nodiscard]] CameraVisibleUpdate getCameraVisibility() const noexcept;

//         private:
//             u8 pos_x          : 6;
//             u8 shadow_casting : 1;
//             u8 camera_visible : 1;

//             u8 pos_y : 6;

//             u8 pos_z : 6;

//             std::array<u8, 2> material;
//         };
//         static_assert(sizeof(ChunkLocalUpdate) == 5);
//     public:
//         explicit ChunkRenderManager(const game::Game*);
//         ~ChunkRenderManager();

//         ChunkRenderManager(const ChunkRenderManager&)             = delete;
//         ChunkRenderManager(ChunkRenderManager&&)                  = delete;
//         ChunkRenderManager& operator= (const ChunkRenderManager&) = delete;
//         ChunkRenderManager& operator= (ChunkRenderManager&&)      = delete;

//         /// Creates an empty chunk at the world aligned position
//         [[nodiscard]] Chunk createChunk(WorldPosition);
//         void                destroyChunk(Chunk);

//         PointLight
//              createPointLight(glm::vec3 position, glm::vec4 colorAndPower, glm::vec4 falloffs);
//         void destroyPointLight(PointLight);

//         void updateChunkVoxels(const Chunk&, std::span<ChunkLocalUpdate>);

//     private:
//     };
// } // namespace old_voxel

// // // dump updates
// // // each frame
// // // step along cycle:
// // // collect updates -> process updates
// // // States:
// // // WaitingForUpdates
// // // ProcessingUpdates