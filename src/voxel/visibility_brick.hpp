#pragma once

#include "util/log.hpp"
#include "util/misc.hpp"
#include "voxel/constants.hpp"
#include <array>
#include <limits>

namespace voxel
{
    struct VisibilityBrick
    {
        void fill(bool filled)
        {
            std::fill(this->data.begin(), this->data.end(), filled ? ~0u : 0u);
        }

        void iterateOverVoxels(
            std::invocable<BrickLocalPosition, bool> auto func) const
        {
            for (int x = 0; x < 8; ++x)
            {
                for (int y = 0; y < 8; ++y)
                {
                    for (int z = 0; z < 8; ++z)
                    {
                        const BrickLocalPosition p {glm::u8vec3 {x, y, z}};

                        func(p, this->read(p));
                    }
                }
            }
        }

        [[nodiscard]] bool read(BrickLocalPosition p) const
        {
            const auto [idx, bit] = getIdxAndBitOfBrickLocalPosition(p);

            return (this->data[idx] & (1u << bit)) != 0; // NOLINT
        }

        void write(BrickLocalPosition p, bool filled)
        {
            const auto [idx, bit] = getIdxAndBitOfBrickLocalPosition(p);

            if (filled)
            {
                this->data[idx] |= (1u << bit); // NOLINT
            }
            else
            {
                this->data[idx] &= ~(1u << bit); // NOLINT
            }
        }

        [[nodiscard]]
        static std::pair<std::size_t, std::size_t>
        getIdxAndBitOfBrickLocalPosition(BrickLocalPosition p)
        {
            if constexpr (util::isDebugBuild())
            {
                util::assertFatal(p.x < BrickEdgeLengthVoxels, "{}", p.x);
                util::assertFatal(p.y < BrickEdgeLengthVoxels, "{}", p.y);
                util::assertFatal(p.z < BrickEdgeLengthVoxels, "{}", p.z);
            }

            const std::size_t linearIndex =
                p.x + p.z * BrickEdgeLengthVoxels
                + p.y * BrickEdgeLengthVoxels * BrickEdgeLengthVoxels;

            return {
                linearIndex / std::numeric_limits<u32>::digits,
                linearIndex % std::numeric_limits<u32>::digits,
            };
        }

        std::array<u32, 16> data;
    };
} // namespace voxel
