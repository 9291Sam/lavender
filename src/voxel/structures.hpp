#pragma once

#include "util/log.hpp"
#include "util/misc.hpp"
#include <ctti/nameof.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/string_cast.hpp>

namespace voxel
{
    /// Each voxel chunk is a 64^3 volume
    /// it is subdivided into smaller chunks of size 8^3, it contains 8^3 of these

    static constexpr u8 VoxelsPerChunkEdge = 64;
    static constexpr u8 BricksPerChunkEdge = 8;
    static constexpr u8 VoxelsPerBrickEdge = 8;

    struct UncheckedInDebugTag
    {};

    template<class Derived, class V, std::size_t Bound = static_cast<std::size_t>(-1)>
    struct VoxelCoordinateBase : V
    {
        explicit VoxelCoordinateBase(V v) // NOLINT
            : V {v}
        {
            (*this) = VoxelCoordinateBase<Derived, V, Bound>::tryCreate(v);
        }

        explicit VoxelCoordinateBase(V v, UncheckedInDebugTag) // NOLINT
            : V {v}
        {}

        static std::optional<Derived> tryCreate(V v)
        {
            if constexpr (util::isDebugBuild() && Bound != static_Cast<std::size_t>(-1))
            {
                static_assert(V::length() == 3);

                if (v.x >= Bound || v.y >= Bound || v.z >= Bound)
                {
                    return std::nullopt;
                }
            }

            return v;
        }

        static Derived fromLinearIndex(std::size_t linearIndex)
        {
            static_assert(Bound != -1);

            const typename V::type z = static_cast<V::type>(linearIndex / (Bound * Bound));
            const typename V::type y = (linearIndex / Bound) % Bound;
            const typename V::type x = linearIndex % Bound;

            return Derived {V {x, y, z}};
        }

        [[nodiscard]] V asVector() const noexcept
        {
            return *this;
        }

        [[nodiscard]] std::size_t asLinearIndex() const noexcept
        {
            return this->x + (Bound * this->y) + (Bound * Bound * this->z);
        }
    };

    /// Represents the position of a single voxel in world space
    struct WorldPosition : public VoxelCoordinateBase<WorldPosition, glm::i32vec3>
    {};

    /// Represents the position of a single voxel in the space of a generic chunk
    struct ChunkLocalPosition
        : public VoxelCoordinateBase<ChunkLocalPosition, glm::u8vec3, VoxelsPerChunkEdge>
    {};

    /// Represents the position of a single voxel in the space of a generic brick
    struct BrickLocalPosition
        : public VoxelCoordinateBase<BrickLocalPosition, glm::u8vec3, VoxelsPerBrickEdge>
    {};

    /// Represents the position of a single Brick in the space of a chunk
    struct BrickCoordinate
        : public VoxelCoordinateBase<BrickCoordinate, glm::u8vec3, BricksPerChunkEdge>
    {};

    enum class Voxel : u16 // NOLINT
    {
        NullAirEmpty = 0,
        Emerald,
        Ruby,
        Pearl,
        Obsidian,
        Brass,
        Chrome,
        Copper,
        Gold,
        Topaz,
        Sapphire,
        Amethyst,
        Jade,
        Diamond
    };

    enum class VoxelFaceDirection : std::uint8_t
    {
        Top    = 0,
        Bottom = 1,
        Left   = 2,
        Right  = 3,
        Front  = 4,
        Back   = 5
    };

    inline glm::i8vec3 getDirFromDirection(VoxelFaceDirection dir)
    {
        switch (dir)
        {
        case VoxelFaceDirection::Top:
            return glm::i8vec3 {0, 1, 0};
        case VoxelFaceDirection::Bottom:
            return glm::i8vec3 {0, -1, 0};
        case VoxelFaceDirection::Left:
            return glm::i8vec3 {-1, 0, 0};
        case VoxelFaceDirection::Right:
            return glm::i8vec3 {1, 0, 0};
        case VoxelFaceDirection::Front:
            return glm::i8vec3 {0, 0, -1};
        case VoxelFaceDirection::Back:
            return glm::i8vec3 {0, 0, 1};
        default:
            util::panic(
                "voxel::getDirFromDirection(VoxelFaceDirection) passed invalid "
                "direction | Value: {}",
                util::toUnderlying(dir));
        }
    }

    // Width, height, and ascension axes
    inline std::tuple<glm::i8vec3, glm::i8vec3, glm::i8vec3> getDrivingAxes(VoxelFaceDirection dir)
    {
        switch (dir)
        {
        case VoxelFaceDirection::Top:
            [[fallthrough]];
        case VoxelFaceDirection::Bottom:
            return {
                glm::i8vec3 {1, 0, 0},
                glm::i8vec3 {0, 0, 1},
                glm::i8vec3 {0, 1, 0},
            };
        case VoxelFaceDirection::Left:
            [[fallthrough]];
        case VoxelFaceDirection::Right:
            return {
                glm::i8vec3 {0, 1, 0},
                glm::i8vec3 {0, 0, 1},
                glm::i8vec3 {1, 0, 0},
            };
        case VoxelFaceDirection::Front:
            [[fallthrough]];
        case VoxelFaceDirection::Back:
            return {
                glm::i8vec3 {1, 0, 0},
                glm::i8vec3 {0, 1, 0},
                glm::i8vec3 {0, 0, 1},
            };
        default:
            util::panic(
                "voxel::getDrivingAxes(VoxelFaceDirection) passed invalid "
                "direction | Value: {}",
                util::toUnderlying(dir));
        }
    }

    // Represents a generic brick with boolean values
    struct BitBrick
    {
        std::array<u32, 16> data;

        void fill(bool b)
        {
            std::ranges::fill(this->data, b ? ~u32 {0} : 0);
        }

        void write(BrickLocalPosition p, bool b)
        {
            const auto [idx, bit] = BitBrick::getIdxAndBit(p);

            if (b)
            {
                this->data[idx] |= (1u << bit); // NOLINT
            }
            else
            {
                this->data[idx] &= ~(1u << bit); // NOLINT
            }
        }

        [[nodiscard]] bool read(BrickLocalPosition p) const
        {
            const auto [idx, bit] = BitBrick::getIdxAndBit(p);

            return (this->data[idx] & (1u << bit)) != 0; // NOLINT
        }

        [[nodiscard]] static std::pair<std::size_t, std::size_t> getIdxAndBit(BrickLocalPosition p)
        {
            const std::size_t linearIndex = p.asLinearIndex();

            return {
                linearIndex / std::numeric_limits<u32>::digits,
                linearIndex % std::numeric_limits<u32>::digits};
        }
    };

    // Represents a Brick with differing values per face
    struct VisibilityBrick
    {
        std::array<BitBrick, 6> data;
    };

    template<class T>
    struct TypedBrick
    {
        std::array<
            std::array<std::array<T, VoxelsPerBrickEdge>, VoxelsPerBrickEdge>,
            VoxelsPerBrickEdge>
            data;

        void fill(T t)
        {
            for (std::size_t i = 0; i < VoxelsPerBrickEdge; ++i)
            {
                for (std::size_t j = 0; j < VoxelsPerBrickEdge; ++j)
                {
                    for (std::size_t k = 0; k < VoxelsPerBrickEdge; ++k)
                    {
                        this->data[i][j][k] = t; // NOLINT
                    }
                }
            }
        }

        void write(BrickLocalPosition p, T t)
        {
            this->data[p.x][p.y][p.z] = t; // NOLINT
        }

        [[nodiscard]] T read(BrickLocalPosition p) const
        {
            return this->data[p.x][p.y][p.z]; // NOLINT
        }

        [[nodiscard]] T& modify(BrickLocalPosition p)
        {
            return this->data[p.x][p.y][p.z]; // NOLINT
        }
    };

    struct MaterialBrick : TypedBrick<Voxel>
    {};

    struct GreedyVoxelFace
    {
        u32 x      : 6;
        u32 y      : 6;
        u32 z      : 6;
        u32 width  : 6;
        u32 height : 6;
        u32 pad    : 2;
    };
    static_assert(std::is_trivially_copyable_v<GreedyVoxelFace>);

    struct DenseBitChunk
    {
        std::array<std::array<u64, VoxelsPerChunkEdge>, VoxelsPerChunkEdge> data;

        static bool isPositionInBounds(glm::i8vec3 p)
        {
            return p.x >= 0 && p.x < VoxelsPerChunkEdge && p.y >= 0 && p.y < VoxelsPerChunkEdge
                && p.z >= 0 && p.z < VoxelsPerChunkEdge;
        }

        // returns false on out of bounds access
        [[nodiscard]] bool isOccupied(glm::i8vec3 p) const
        {
            if (p.x < 0 || p.x >= VoxelsPerChunkEdge || p.y < 0 || p.y >= VoxelsPerChunkEdge
                || p.z < 0 || p.z >= VoxelsPerChunkEdge)
            {
                return false;
            }
            else
            {
                return static_cast<bool>(
                    this->data[static_cast<std::size_t>(p.x)] // NOLINT
                              [static_cast<std::size_t>(p.y)] // NOLINT
                    & (1ULL << static_cast<u64>(p.z)));
            }
        }
        // if its occupied, it removes it from the data structure
        [[nodiscard]] bool isOccupiedClearing(glm::i8vec3 p)
        {
            const bool result = this->isOccupied(p);

            if (result)
            {
                this->write(p, false);
            }

            return result;
        }

        [[nodiscard]] bool
        isEntireRangeOccupied(glm::i8vec3 base, glm::i8vec3 step, i8 range) const // NOLINT
        {
            bool isEntireRangeOccupied = true;

            for (i8 i = 0; i < range; ++i)
            {
                glm::i8vec3 probe = base + step * i;

                if (!this->isOccupied(probe))
                {
                    isEntireRangeOccupied = false;
                    break;
                }
            }

            return isEntireRangeOccupied;
        }

        void clearEntireRange(glm::i8vec3 base, glm::i8vec3 step, i8 range)
        {
            for (i8 i = 0; i < range; ++i)
            {
                glm::i8vec3 probe = base + step * i;

                this->write(probe, false);
            }
        }

        void write(glm::i8vec3 p, bool filled)
        {
            if constexpr (util::isDebugBuild())
            {
                util::assertFatal(
                    p.x >= 0 && p.x < 64 && p.y >= 0 && p.y < 64 && p.z >= 0 && p.z < 64,
                    "{} {} {}",
                    p.x,
                    p.y,
                    p.z);
            }

            if (filled)
            {
                // NOLINTNEXTLINE
                this->data[static_cast<std::size_t>(p.x)][static_cast<std::size_t>(p.y)] |=
                    (u64 {1} << static_cast<u64>(p.z));
            }
            else
            {
                // NOLINTNEXTLINE
                this->data[static_cast<std::size_t>(p.x)][static_cast<std::size_t>(p.y)] &=
                    ~(u64 {1} << static_cast<u64>(p.z));
            }
        }
    };

} // namespace voxel