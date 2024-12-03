

#include "voxel/constants.hpp"
#include "voxel/voxel.hpp"

class Low
{
    // allocates aligned chunks of a fixed size

    // Chunk allocateChunk(ChunkCoordinate);

    // struct MaterialWrite
    // {
    //     voxel::Voxel material;
    //     voxel::ChunkLocalPosition position
    // };

    enum class WriteType
    {
        InvisiblePhysicsWall,
        InvisibleLightBlocker,
        BothBlocker,
        VoxelWoShadowOrPhysics,
    };

    enum class ShadowBitUpdate : std::uint8_t
    {
        NoChange,
        MakeBlocking,
        MakeClear
    };

    enum class PrimaryRayUpdate : std::uint8_t
    {
        NoChange,
        MakeVisible,
        MakeInvisible,
    };

    class ChunkUpdate
    {
        ChunkUpdate(
            voxel::ChunkLocalPosition,
            std::optional<voxel::Voxel> updateVoxel,
            PrimaryRayUpdate,
            ShadowBitUpdate);
        ~ChunkUpdate() = default;

        ChunkUpdate(const ChunkUpdate&)             = default;
        ChunkUpdate(ChunkUpdate&&)                  = default;
        ChunkUpdate& operator= (const ChunkUpdate&) = default;
        ChunkUpdate& operator= (ChunkUpdate&&)      = default;

        [[nodiscard]] std::tuple<
            voxel::ChunkLocalPosition,
            std::optional<voxel::ChunkLocalPosition>,
            std::optional<voxel::Voxel>,
            PrimaryRayUpdate,
            ShadowBitUpdate>
        unpack() const;

    private:
        u8 pos_x         : 6;
        u8 shadow_ignore : 1;
        u8 shadow_write  : 1;

        u8 pos_y      : 6;
        u8 ray_ignore : 1;
        u8 ray_write  : 1;

        u8 pos_z           : 6;
        u8 ignore_material : 1;

        u8 material_upper;
        u8 material_lower;
    };

    // physics is a higher level thing, doesnt need to be here

    // material buffer
    // material primary ray bit brick (and raster buffer)
    // material secondary ray bit brick (and raster buffer)

    // primary buffer contains all secondary ray buffer
    // PrimaryAndSecondary, SecondaryOnly

    // shadow bit brick (raster buffer)

    // normal
    // material shadow
    // 00 - empty
    // 01 - invisible light wall
    // 10 - non shadow casting voxel (grass)
    // 11 - shadow casting voxel (normal)

    // tracking things
    // 00
    // 01 (covered by other)
    // 10 - material is there but not
    // invisible material (but material is there)
    // tracking an object thats rendered somwhere else

    // + material buffer
    // +

    // Material, Shadow, Ray;
    // 000 - empty
    // 001 - doesn't make sense
    // 010 - invisible light blocker
    // 011 - doesn't make sense
    // 100 - doesn't make sense
    // 101 - render a voxel w/o physics or shadow
    // 110 - invisible, shadow casting voxel (useful for tracking rotated)
    // 111 - everything

    // enum class WriteType : std::uint8_t
    // {
    //     // Material, Shadow, Physics, Ray
    //     Material
    // }

    // void writeToChunk(std::span<MaterialWrite>)
};