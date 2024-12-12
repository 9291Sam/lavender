

// #include "voxel/constants.hpp"
// #include "voxel/voxel.hpp"
// #include <span>

namespace voxel2
{
    // TODO: Invisible Tracking Info for non voxel things
    // TODO: Transparency & translucency & colored glass light color changing.

    class GpuVoxelDataManager
    {
    public:
        class ChunkUpdate
        {
        public:

            enum class ShadowUpdate : std::uint8_t
            {
                ShadowCasting,
                ShadowTransparent
            };

            struct VoxelUpdate
            {
                voxel::Voxel voxel;
                ShadowUpdate shadow_update;
            };

        public:
            ChunkUpdate(voxel::ChunkLocalPosition, std::optional<VoxelUpdate>);
            ~ChunkUpdate() = default;

            ChunkUpdate(const ChunkUpdate&)             = default;
            ChunkUpdate(ChunkUpdate&&)                  = default;
            ChunkUpdate& operator= (const ChunkUpdate&) = default;
            ChunkUpdate& operator= (ChunkUpdate&&)      = default;

            [[nodiscard]] std::tuple<voxel::ChunkLocalPosition, std::optional<VoxelUpdate>>
            unpack() const;

        private:
            u8 pos_x              : 6;
            u8 has_voxel_update   : 1;
            u8 should_shadow_cast : 1;

            u8 pos_y : 6;

            u8 pos_z : 6;

            u8 material_upper;
            u8 material_lower;
        };

        struct Chunk
        {};

        [[nodiscard]] Chunk allocateChunk() const;
        void                freeChunk() const;

        void enqueueChunkUpdates(const Chunk&, std::vector<ChunkUpdate>) const;

        void processChunkUpdates();
    };

    // enqueue updates
    // create a mesh package
    // mesh it
    // upload to gpu

    class High
    {
        // write to position.

        // write and read from global positions

        // TODO: transactions

        VoxelTransaction createTransaction();

        // transactions stack on top of one another
        // have an order that is the order they were inserted into the chunk
    };

    class High
    {};
} // namespace voxel2

// class Low
// {
//     // Primary Material Ray (makes things show up on screen)
//     // Secondary Material Ray (makes things have GI effects on the environment, implies shadow)

//     // Primary / Secondary Ray distinction.
//     // Primary rays are what show up on the monitor
//     // Secondary rays are what are used to calculate shadows, GI effects etc.

//     enum class ShadowBitUpdate : std::uint8_t
//     {
//         NoChange,
//         MakeBlocking,
//         MakeClear
//     };

//     enum class PrimaryRayUpdate : std::uint8_t
//     {
//         NoChange,
//         MakeVisible,
//         MakeInvisible,
//     };

//     // physics is a higher level thing, doesnt need to be here

//     // material buffer
//     // material primary ray bit brick (and raster buffer)
//     // material secondary ray bit brick (and raster buffer)

//     // primary buffer contains all secondary ray buffer
//     // PrimaryAndSecondary, SecondaryOnly

//     // shadow bit brick (raster buffer)

//     enum class VoxelState
//     {
//         Empty,
//         InvisibleWall,
//         NonShadowCastingVisible, // imagine grass on the ground
//         ShadowCastingVisible,    // normal
//         InvisibleTracker,        // a non world aligned entity that needs shadows and GI
//     };

//     // how to do transparent glass?
//     // you'd need to split it up into three bit maps per voxel
//     // is there something here (this is the one that is actually traced)
//     // is this transparent or translucent

//     // normal
//     // material shadow
//     // 00 - empty
//     // 01 - invisible light wall
//     // 10 - non shadow casting voxel (grass)
//     // 11 - shadow casting voxel (normal)

//     // tracking things
//     // 00 - empty
//     // 01 - invisible wall (covered by other)
//     // 10 - material is there but not interactable (cant exist, how would GI effects happen if
//     they
//     // werent transparent to shadows) 11 - casts shadow, has gi effects, not visible invisible
//     // 11 material (but material is there) tracking an object thats rendered somwhere else

//     // + material buffer
//     // +

//     // Material, Shadow, Ray;
//     // 000 - empty
//     // 001 - doesn't make sense
//     // 010 - invisible light blocker
//     // 011 - doesn't make sense
//     // 100 - doesn't make sense
//     // 101 - render a voxel w/o physics or shadow
//     // 110 - invisible, shadow casting voxel (useful for tracking rotated)
//     // 111 - everything

//     // enum class WriteType : std::uint8_t
//     // {
//     //     // Material, Shadow, Physics, Ray
//     //     Material
//     // }

//     // void writeToChunk(std::span<MaterialWrite>)
// };