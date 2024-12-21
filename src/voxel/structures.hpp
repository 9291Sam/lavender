#pragma once

#include "util/log.hpp"
#include "util/misc.hpp"
#include "util/opaque_integer_handle.hpp"
#include "util/range_allocator.hpp"
#include <compare>
#include <ctti/nameof.hpp>
#include <future>
#include <glm/fwd.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/string_cast.hpp>
#include <limits>
#include <utility>

namespace voxel
{
    /// Each voxel chunk is a 64^3 volume
    /// it is subdivided into smaller chunks of size 8^3, it contains 8^3 of these

    static constexpr u8 VoxelsPerChunkEdge = 64;
    static constexpr u8 BricksPerChunkEdge = 8;
    static constexpr u8 VoxelsPerBrickEdge = 8;

    struct UncheckedInDebugTag
    {};

    template<class Derived, class V, V::value_type Bound = static_cast<V::value_type>(-1)>
    struct VoxelCoordinateBase : V
    {
        using Base       = VoxelCoordinateBase;
        using VectorType = V; // NOLINT

        explicit VoxelCoordinateBase(V v) // NOLINT
            : V {v}
        {
            if constexpr (util::isDebugBuild() && Bound != static_cast<V::value_type>(-1))
            {
                if (v.x >= Bound || v.y >= Bound || v.z >= Bound)
                {
                    util::panic("{} {} {} oops", v.x, v.y, v.z);
                }
            }
        }

        explicit VoxelCoordinateBase(V v, UncheckedInDebugTag) // NOLINT
            : V {v}
        {}

        static std::optional<Derived> tryCreate(V v)
        {
            if constexpr (util::isDebugBuild() && Bound != static_cast<std::size_t>(-1))
            {
                static_assert(V::length() == 3);

                if (v.x >= Bound || v.y >= Bound || v.z >= Bound)
                {
                    return std::nullopt;
                }
            }

            return std::optional<Derived> {v, UncheckedInDebugTag {}};
        }

        static Derived fromLinearIndex(std::size_t linearIndex)
        {
            static_assert(Bound != static_cast<V::value_type>(-1));

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

        constexpr std::strong_ordering operator<=> (const VoxelCoordinateBase& other) const
        {
            return std::tie(this->x, this->y, this->z) <=> std::tie(other.x, other.y, other.z);
        }
    };

    /// Represents the position of a single voxel in world space
    struct WorldPosition : public VoxelCoordinateBase<WorldPosition, glm::i32vec3>
    {
        explicit WorldPosition(Base::VectorType v)
            : Base {v}
        {}
    };

    /// Represents the position of a single voxel in the space of a generic chunk
    struct ChunkLocalPosition
        : public VoxelCoordinateBase<ChunkLocalPosition, glm::u8vec3, VoxelsPerChunkEdge>
    {
        explicit ChunkLocalPosition(Base::VectorType v)
            : Base {v}
        {}
    };

    /// Represents the position of a single voxel in the space of a generic brick
    struct BrickLocalPosition
        : public VoxelCoordinateBase<BrickLocalPosition, glm::u8vec3, VoxelsPerBrickEdge>
    {
        explicit BrickLocalPosition(Base::VectorType v)
            : Base {v}
        {}
    };

    /// Represents the position of a single Brick in the space of a chunk
    struct BrickCoordinate
        : public VoxelCoordinateBase<BrickCoordinate, glm::u8vec3, BricksPerChunkEdge>
    {
        explicit BrickCoordinate(Base::VectorType v)
            : Base {v}
        {}
    };

    inline ChunkLocalPosition assembleChunkLocalPosition(BrickCoordinate c, BrickLocalPosition p)
    {
        return ChunkLocalPosition {
            glm::u8vec3 {
                c.x * VoxelsPerBrickEdge, c.y * VoxelsPerBrickEdge, c.z * VoxelsPerBrickEdge}
            + p};
    }

    inline std::pair<BrickCoordinate, BrickLocalPosition>
    splitChunkLocalPosition(ChunkLocalPosition p)
    {
        return {
            BrickCoordinate {p / BricksPerChunkEdge}, BrickLocalPosition {p % BricksPerChunkEdge}};
    }

    inline std::pair<WorldPosition, ChunkLocalPosition> splitWorldPosition(WorldPosition p)
    {
        return {
            WorldPosition {
                {util::divideEuclidean(p.x, static_cast<i32>(VoxelsPerChunkEdge)),
                 util::divideEuclidean(p.y, static_cast<i32>(VoxelsPerChunkEdge)),
                 util::divideEuclidean(p.z, static_cast<i32>(VoxelsPerChunkEdge))}},
            ChunkLocalPosition {
                {util::moduloEuclidean(p.x, static_cast<i32>(VoxelsPerChunkEdge)),
                 util::moduloEuclidean(p.y, static_cast<i32>(VoxelsPerChunkEdge)),
                 util::moduloEuclidean(p.z, static_cast<i32>(VoxelsPerChunkEdge))}}};
    }

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
        Diamond,
        Marble,
        Granite,
        Basalt,
        Limestone,
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

        void iterateOverVoxels(std::invocable<BrickLocalPosition, bool> auto func) const
        {
            for (std::size_t x = 0; x < VoxelsPerBrickEdge; ++x)
            {
                for (std::size_t y = 0; y < VoxelsPerBrickEdge; ++y)
                {
                    for (std::size_t z = 0; z < VoxelsPerBrickEdge; ++z)
                    {
                        const BrickLocalPosition p {glm::u8vec3 {x, y, z}};

                        func(p, this->read(p));
                    }
                }
            }
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

        void iterateOverVoxels(std::invocable<BrickLocalPosition, T> auto func) const
        {
            for (std::size_t x = 0; x < VoxelsPerBrickEdge; ++x)
            {
                for (std::size_t y = 0; y < VoxelsPerBrickEdge; ++y)
                {
                    for (std::size_t z = 0; z < VoxelsPerBrickEdge; ++z)
                    {
                        const BrickLocalPosition p {glm::u8vec3 {x, y, z}};

                        func(p, this->read(p));
                    }
                }
            }
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

    struct MaybeBrickPointer
    {
        static constexpr u32 Null = static_cast<u32>(-1);

        [[nodiscard]] bool isNull() const
        {
            return this->pointer == Null;
        }

        u32 pointer = Null;
    };

    struct BrickPointer : MaybeBrickPointer
    {};

    class ChunkLocalUpdate
    {
    public:
        enum class ShadowUpdate : u8
        {
            ShadowTransparent = 0,
            ShadowCasting     = 1,
        };

        enum class CameraVisibleUpdate : u8
        {
            CameraInvisible = 0,
            CameraVisible   = 1,
        };
    public:
        ChunkLocalUpdate(ChunkLocalPosition p, Voxel v, ShadowUpdate s, CameraVisibleUpdate c)
            : pos_x {p.x}
            , shadow_casting {std::to_underlying(s)}
            , camera_visible {std::to_underlying(c)}
            , pos_y {p.y}
            , pos_z {p.z}
            , material {std::bit_cast<std::array<u8, 2>>(v)}
        {}
        ~ChunkLocalUpdate() = default;

        ChunkLocalUpdate(const ChunkLocalUpdate&)             = default;
        ChunkLocalUpdate(ChunkLocalUpdate&&)                  = default;
        ChunkLocalUpdate& operator= (const ChunkLocalUpdate&) = default;
        ChunkLocalUpdate& operator= (ChunkLocalUpdate&&)      = default;

        [[nodiscard]] ChunkLocalPosition getPosition() const noexcept
        {
            return ChunkLocalPosition {{this->pos_x, this->pos_y, this->pos_z}};
        }
        [[nodiscard]] Voxel getVoxel() const noexcept
        {
            return std::bit_cast<Voxel>(this->material);
        }
        [[nodiscard]] ShadowUpdate getShadowUpdate() const noexcept
        {
            return static_cast<ShadowUpdate>(this->shadow_casting);
        }
        [[nodiscard]] CameraVisibleUpdate getCameraVisibility() const noexcept
        {
            return static_cast<CameraVisibleUpdate>(this->camera_visible);
        }

    private:
        u8 pos_x          : 6;
        u8 shadow_casting : 1;
        u8 camera_visible : 1;

        u8 pos_y : 6;

        u8 pos_z : 6;

        std::array<u8, 2> material;
    };
    static_assert(sizeof(ChunkLocalUpdate) == 5);

    struct ChunkBrickMap
    {
        static constexpr u16 NullOffset = std::numeric_limits<u16>::max();

        ChunkBrickMap()
        {
            for (auto& xyz : this->data)
            {
                for (auto& yz : xyz)
                {
                    for (u16& z : yz)
                    {
                        z = NullOffset;
                    }
                }
            }
        }

        [[nodiscard]] std::optional<u16> getMaxValidOffset() const
        {
            std::optional<u16> maxOffset {};

            for (auto& xyz : this->data)
            {
                for (auto& yz : xyz)
                {
                    for (u16 z : yz)
                    {
                        if (z != NullOffset)
                        {
                            if (maxOffset.has_value())
                            {
                                maxOffset = std::max({z, *maxOffset});
                            }
                            else
                            {
                                maxOffset = z;
                            }
                        }
                    }
                }
            }

            return maxOffset;
        }

        void iterateOverBricks(std::invocable<BrickCoordinate, u16> auto func) const
        {
            for (std::size_t x = 0; x < BricksPerChunkEdge; ++x)
            {
                for (std::size_t y = 0; y < BricksPerChunkEdge; ++y)
                {
                    for (std::size_t z = 0; z < BricksPerChunkEdge; ++z)
                    {
                        const BrickCoordinate p {glm::u8vec3 {x, y, z}};

                        func(p, this->data[x][y][z]); // NOLINT
                    }
                }
            }
        }

        void setOffset(BrickCoordinate bC, u16 offset)
        {
            this->data[bC.x][bC.y][bC.z] = offset; // NOLINT
        }

        u16 getOffset(BrickCoordinate bC)
        {
            return this->data[bC.x][bC.y][bC.z]; // NOLINT
        }

        std::array<
            std::array<std::array<u16, BricksPerChunkEdge>, BricksPerChunkEdge>,
            BricksPerChunkEdge>
            data;
    };

    struct ChunkAsyncMesh
    {
        // every time you add something to this you need to free it in destroyChunk
    };

    struct CpuChunkData
    {
        std::optional<util::RangeAllocation>                active_brick_range_allocation;
        std::optional<std::array<util::RangeAllocation, 6>> active_draw_allocations;

        std::vector<ChunkLocalUpdate> updates;
    };

    struct PerChunkGpuData
    {
        i32           world_offset_x          = 0;
        i32           world_offset_y          = 0;
        i32           world_offset_z          = 0;
        u32           brick_allocation_offset = 0;
        ChunkBrickMap data;
    };

    struct BrickParentInformation
    {
        u32 parent_chunk             : 16;
        u32 position_in_parent_chunk : 9;
    };

    struct VisibleFaceIdBrickHashMapStorage
    {
        u32 key;
        u32 value;
    };

    struct VisibleFaceData
    {
        u32       data;
        glm::vec3 calculated_color;
    };

    struct GlobalVoxelData
    {
        u32 number_of_visible_faces;
        u32 number_of_calculating_draws_x;
        u32 number_of_calculating_draws_y;
        u32 number_of_calculating_draws_z;
        u32 number_of_lights;
        u32 readback_number_of_visible_faces;
    };

    struct GpuRaytracedLight
    {
        glm::vec4 position_and_half_intensity_distance;
        glm::vec4 color_and_power;
    };

} // namespace voxel

template<>
struct std::hash<voxel::WorldPosition> // NOLINT
{
    std::size_t operator() (const voxel::WorldPosition& c) const
    {
        return std::hash<voxel::WorldPosition::VectorType> {}(
            static_cast<voxel::WorldPosition::VectorType>(c));
    }
};
