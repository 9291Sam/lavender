#pragma once

#include "util/log.hpp"
#include "util/misc.hpp"
#include <glm/fwd.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/vec3.hpp>
#include <glm/vector_relational.hpp>
#include <optional>

namespace old_voxel
{
    static constexpr u8 ChunkEdgeLengthVoxels = 64;
    static constexpr u8 ChunkEdgeLengthBricks = 8;
    static constexpr u8 BrickEdgeLengthVoxels = 8;

    struct WorldPosition : public glm::i32vec3
    {};

    // Represents a position within a Brick
    struct BrickLocalPosition : public glm::u8vec3
    {
        explicit BrickLocalPosition(glm::u8vec3 pos)
            : glm::u8vec3 {pos}
        {
            if (util::isDebugBuild())
            {
                util::assertFatal(pos.x < BrickEdgeLengthVoxels, "BrickLocalPosition X {}", pos.x);
                util::assertFatal(pos.y < BrickEdgeLengthVoxels, "BrickLocalPosition Y {}", pos.y);
                util::assertFatal(pos.z < BrickEdgeLengthVoxels, "BrickLocalPosition Z {}", pos.z);
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
        explicit BrickCoordinate(glm::u8vec3 pos)
            : glm::u8vec3 {pos}
        {
            if (util::isDebugBuild())
            {
                util::assertFatal(pos.x < ChunkEdgeLengthBricks, "BrickCoordinate X {}", pos.x);
                util::assertFatal(pos.y < ChunkEdgeLengthBricks, "BrickCoordinate Y {}", pos.y);
                util::assertFatal(pos.z < ChunkEdgeLengthBricks, "BrickCoordinate Z {}", pos.z);
            }
        }

        static std::optional<BrickCoordinate> tryCreate(glm::u8vec3 pos)
        {
            if (pos.x >= ChunkEdgeLengthBricks || pos.y >= ChunkEdgeLengthBricks
                || pos.z >= ChunkEdgeLengthBricks)
            {
                return std::nullopt;
            }
            else
            {
                return BrickCoordinate {pos};
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
        explicit ChunkLocalPosition(glm::u8vec3 pos)
            : glm::u8vec3 {pos}
        {
            if (util::isDebugBuild())
            {
                util::assertFatal(pos.x < ChunkEdgeLengthVoxels, "ChunkLocalPosition X {}", pos.x);
                util::assertFatal(pos.y < ChunkEdgeLengthVoxels, "ChunkLocalPosition Y {}", pos.y);
                util::assertFatal(pos.z < ChunkEdgeLengthVoxels, "ChunkLocalPosition Z {}", pos.z);
            }
        }

        static std::optional<ChunkLocalPosition> tryCreate(glm::u8vec3 pos)
        {
            if (pos.x >= ChunkEdgeLengthVoxels || pos.y >= ChunkEdgeLengthVoxels
                || pos.z >= ChunkEdgeLengthVoxels)
            {
                return std::nullopt;
            }
            else
            {
                return ChunkLocalPosition {pos};
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

    inline WorldPosition getWorldPositionOfChunkCoordinate(ChunkCoordinate c)
    {
        return WorldPosition {c * 64};
    }

    inline std::pair<ChunkCoordinate, ChunkLocalPosition> splitWorldPosition(WorldPosition p)
    {
        return {
            ChunkCoordinate {
                {util::divideEuclidean<i32>(p.x, ChunkEdgeLengthVoxels),
                 util::divideEuclidean<i32>(p.y, ChunkEdgeLengthVoxels),
                 util::divideEuclidean<i32>(p.z, ChunkEdgeLengthVoxels)}},
            ChunkLocalPosition {
                {static_cast<u8>(util::moduloEuclidean<i32>(p.x, ChunkEdgeLengthVoxels)),
                 static_cast<u8>(util::moduloEuclidean<i32>(p.y, ChunkEdgeLengthVoxels)),
                 static_cast<u8>(util::moduloEuclidean<i32>(p.z, ChunkEdgeLengthVoxels))}}};
    }

    inline std::pair<BrickCoordinate, BrickLocalPosition>
    splitChunkLocalPosition(ChunkLocalPosition p)
    {
        return {
            BrickCoordinate {p / ChunkEdgeLengthBricks},
            BrickLocalPosition {p % ChunkEdgeLengthBricks}};
    }

    inline WorldPosition assembleWorldPosition(ChunkCoordinate c, ChunkLocalPosition p)
    {
        return WorldPosition {
            static_cast<glm::i32vec3>(getWorldPositionOfChunkCoordinate(c))
            + static_cast<glm::i32vec3>(p)};
    }

    inline ChunkLocalPosition assembleChunkLocalPosition(BrickCoordinate c, BrickLocalPosition p)
    {
        return ChunkLocalPosition {glm::u8vec3 {c.x * 8, c.y * 8, c.z * 8} + p};
    }

} // namespace old_voxel

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

    template<>
    struct hash<voxel::WorldPosition>
    {
        std::size_t operator() (const voxel::WorldPosition& c) const
        {
            return std::hash<glm::i32vec3> {}(static_cast<glm::i32vec3>(c));
        }
    };
} // namespace std
