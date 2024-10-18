#pragma once

#include "util/misc.hpp"
#include <glm/fwd.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/vec3.hpp>
#include <glm/vector_relational.hpp>
#include <optional>

namespace voxel
{
    static constexpr u8 ChunkEdgeLengthVoxels = 64;
    static constexpr u8 ChunkEdgeLengthBricks = 8;
    static constexpr u8 BrickEdgeLengthVoxels = 8;

    struct BrickLocalPosition : public glm::u8vec3
    {};

    struct BrickCoordinate : public glm::u8vec3
    {
        [[nodiscard]] u32 getLinearPositionInChunk() const
        {
            return this->x + BrickEdgeLengthVoxels * this->y
                 + BrickEdgeLengthVoxels * BrickEdgeLengthVoxels * this->z;
        }

        static BrickCoordinate fromLinearPositionInChunk(u32 linearIndex)
        {
            const u8 z = static_cast<u8>(
                linearIndex / (BrickEdgeLengthVoxels * BrickEdgeLengthVoxels));
            const u8 y =
                (linearIndex / BrickEdgeLengthVoxels) % BrickEdgeLengthVoxels;
            const u8 x = linearIndex % BrickEdgeLengthVoxels;

            return BrickCoordinate {{x, y, z}};
        }
    };

    struct ChunkLocalPosition : public glm::u8vec3
    {
        explicit operator glm::i8vec3 () const
        {
            return glm::i8vec3 {
                static_cast<i8>(this->x),
                static_cast<i8>(this->y),
                static_cast<i8>(this->z),
            };
        }
    };

    struct ChunkCoordinate : public glm::i32vec3
    {};

    inline std::pair<BrickCoordinate, BrickLocalPosition>
    splitChunkLocalPosition(ChunkLocalPosition p)
    {
        return {
            BrickCoordinate {p / ChunkEdgeLengthBricks},
            BrickLocalPosition {p % ChunkEdgeLengthBricks}};
    }

    inline ChunkLocalPosition
    assembleChunkLocalPosition(BrickCoordinate c, BrickLocalPosition p)
    {
        return ChunkLocalPosition {glm::u8vec3 {c.x * 8, c.y * 8, c.z * 8} + p};
    }

    inline std::optional<ChunkLocalPosition>
    tryMakeChunkLocalPosition(glm::i8vec3 p)
    {
        if (p.x < 0 || p.y < 0 || p.z < 0 || p.x >= ChunkEdgeLengthVoxels
            || p.y >= ChunkEdgeLengthVoxels || p.z >= ChunkEdgeLengthVoxels)
        {
            return std::nullopt;
        }
        else
        {
            return ChunkLocalPosition {p};
        }
    }
} // namespace voxel

namespace std
{
    template<>
    struct hash<voxel::ChunkCoordinate>
    {
        std::size_t operator() (const voxel::ChunkCoordinate& c) const
        {
            return std::hash<glm::i32vec3> {}(static_cast<glm::i32vec3>(c));
        }
    };
} // namespace std