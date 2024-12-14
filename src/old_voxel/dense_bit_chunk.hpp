#pragma once

#include "util/log.hpp"
#include "util/misc.hpp"
#include <array>
#include <glm/gtx/hash.hpp>

namespace old_voxel
{
    struct DenseBitChunk
    {
        std::array<std::array<u64, 64>, 64> data;

        static bool isPositionInBounds(glm::i8vec3 p)
        {
            return p.x >= 0 && p.x < 64 && p.y >= 0 && p.y < 64 && p.z >= 0 && p.z < 64;
        }

        // returns false on out of bounds access
        [[nodiscard]] bool isOccupied(glm::i8vec3 p) const
        {
            if (p.x < 0 || p.x >= 64 || p.y < 0 || p.y >= 64 || p.z < 0 || p.z >= 64)
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
} // namespace old_voxel
