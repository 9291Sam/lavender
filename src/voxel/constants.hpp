#pragma once

#include "util/log.hpp"
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

    struct WorldPosition : public glm::i32vec3
    {};

    // Represents a position within a Brick
    struct BrickLocalPosition : public glm::u8vec3
    {
        explicit BrickLocalPosition(glm::u8vec3 p)
            : glm::u8vec3 {p}
        {
            if (util::isDebugBuild())
            {
                util::assertFatal(p.x < BrickEdgeLengthVoxels, "BrickLocalPosition X {}", p.x);
                util::assertFatal(p.y < BrickEdgeLengthVoxels, "BrickLocalPosition Y {}", p.y);
                util::assertFatal(p.z < BrickEdgeLengthVoxels, "BrickLocalPosition Z {}", p.z);
            }
        }

        static std::optional<BrickLocalPosition> tryCreate(glm::u8vec3 pos)
        {
            if (pos.x >= BrickEdgeLengthVoxels || pos.y >= BrickEdgeLengthVoxels
                || pos.z >= BrickEdgeLengthVoxels)
            {
                return std::nullopt;
            }
            else
            {
                return BrickLocalPosition {pos};
            }
        }
    };

    // Represents a Brick's position within a Chunk
    struct BrickCoordinate : public glm::u8vec3
    {
        explicit BrickCoordinate(glm::u8vec3 p)
            : glm::u8vec3 {p}
        {
            if (util::isDebugBuild())
            {
                util::assertFatal(p.x < ChunkEdgeLengthBricks, "BrickCoordinate X {}", p.x);
                util::assertFatal(p.y < ChunkEdgeLengthBricks, "BrickCoordinate Y {}", p.y);
                util::assertFatal(p.z < ChunkEdgeLengthBricks, "BrickCoordinate Z {}", p.z);
            }
        }

        static std::optional<BrickCoordinate> tryCreate(glm::u8vec3 p)
        {
            if (p.x >= ChunkEdgeLengthBricks || p.y >= ChunkEdgeLengthBricks
                || p.z >= ChunkEdgeLengthBricks)
            {
                return std::nullopt;
            }
            else
            {
                return BrickCoordinate {p};
            }
        }

        [[nodiscard]] u32 getLinearPositionInChunk() const
        {
            return this->x + BrickEdgeLengthVoxels * this->y
                 + BrickEdgeLengthVoxels * BrickEdgeLengthVoxels * this->z;
        }

        static BrickCoordinate fromLinearPositionInChunk(u32 linearIndex)
        {
            const u8 z =
                static_cast<u8>(linearIndex / (BrickEdgeLengthVoxels * BrickEdgeLengthVoxels));
            const u8 y = (linearIndex / BrickEdgeLengthVoxels) % BrickEdgeLengthVoxels;
            const u8 x = linearIndex % BrickEdgeLengthVoxels;

            return BrickCoordinate {{x, y, z}};
        }
    };

    struct ChunkLocalPosition : public glm::u8vec3
    {
        explicit ChunkLocalPosition(glm::u8vec3 p)
            : glm::u8vec3 {p}
        {
            if (util::isDebugBuild())
            {
                util::assertFatal(p.x < ChunkEdgeLengthVoxels, "ChunkLocalPosition X {}", p.x);
                util::assertFatal(p.y < ChunkEdgeLengthVoxels, "ChunkLocalPosition Y {}", p.y);
                util::assertFatal(p.z < ChunkEdgeLengthVoxels, "ChunkLocalPosition Z {}", p.z);
            }
        }

        static std::optional<ChunkLocalPosition> tryCreate(glm::u8vec3 p)
        {
            if (p.x >= ChunkEdgeLengthVoxels || p.y >= ChunkEdgeLengthVoxels
                || p.z >= ChunkEdgeLengthVoxels)
            {
                return std::nullopt;
            }
            else
            {
                return ChunkLocalPosition {p};
            }
        }

        explicit operator glm::i8vec3 () const
        {
            return glm::i8vec3 {
                static_cast<i8>(this->x),
                static_cast<i8>(this->y),
                static_cast<i8>(this->z),
            };
        }
    };

    // Chunk Coordinate in world space ()
    struct ChunkCoordinate : public glm::i32vec3
    {};

    inline std::pair<BrickCoordinate, BrickLocalPosition>
    splitChunkLocalPosition(ChunkLocalPosition p)
    {
        return {
            BrickCoordinate {p / ChunkEdgeLengthBricks},
            BrickLocalPosition {p % ChunkEdgeLengthBricks}};
    }

    inline ChunkLocalPosition assembleChunkLocalPosition(BrickCoordinate c, BrickLocalPosition p)
    {
        return ChunkLocalPosition {glm::u8vec3 {c.x * 8, c.y * 8, c.z * 8} + p};
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
