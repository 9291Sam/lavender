#pragma once

#include "util/log.hpp"
#include <compare>
#include <util/misc.hpp>

namespace voxel
{
    class PointLight
    {
    public:
        static constexpr u32 NullPointLight = static_cast<u32>(-1);
    public:
        PointLight()
            : id {NullPointLight}
        {}
        ~PointLight()
        {
            if (this->id != NullPointLight)
            {
                util::logWarn("Leaked PointLight!");
            }
        }

        PointLight(const PointLight&) = delete;
        PointLight(PointLight&& other) noexcept
            : id {other.id}
        {
            other.id = NullPointLight;
        }
        PointLight& operator= (const PointLight&) = delete;
        PointLight& operator= (PointLight&& other) noexcept
        {
            if (&other != this)
            {
                this->id = other.id;
                other.id = NullPointLight;
            }

            return *this;
        }

        [[nodiscard]] bool isNull() const
        {
            return this->id == NullPointLight;
        }

        std::strong_ordering operator<=> (const PointLight&) const = default;


    private:
        explicit PointLight(u32 id_)
            : id {id_}
        {}
        friend class ChunkManager;

        u32 id;
    };
} // namespace voxel
