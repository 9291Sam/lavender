#pragma once

#include "util/log.hpp"
#include <boost/functional/hash.hpp>
#include <compare>
#include <glm/gtx/hash.hpp>
#include <glm/vec4.hpp>
#include <util/misc.hpp>

namespace voxel
{

    struct InternalPointLight
    {
        glm::vec4 position;
        glm::vec4 color_and_power;
        glm::vec4 falloffs;

        constexpr bool operator== (const InternalPointLight& l) const
        {
            return this->position == l.position
                && this->color_and_power == l.color_and_power
                && this->falloffs == l.falloffs;
        }
    };

    struct InternalPointLightHasher
    {
        constexpr std::size_t operator() (const InternalPointLight& l) const
        {
            std::size_t res = 0;

            boost::hash_combine(res, std::hash<glm::vec4> {}(l.position));
            boost::hash_combine(
                res, std::hash<glm::vec4> {}(l.color_and_power));
            boost::hash_combine(res, std::hash<glm::vec4> {}(l.falloffs));

            return res;
        }
    };

    // TODO: extract into a UniqueId class that can be inherited from
    class PointLight
    {
    public:
        static constexpr u32 NullId = static_cast<u32>(-1);
    public:
        PointLight()
            : id {NullId}
        {}
        ~PointLight()
        {
            if (this->id != NullId)
            {
                util::logWarn("Leaked PointLight!");
            }
        }

        PointLight(const PointLight&) = delete;
        PointLight(PointLight&& other) noexcept
            : id {other.id}
        {
            other.id = NullId;
        }
        PointLight& operator= (const PointLight&) = delete;
        PointLight& operator= (PointLight&& other) noexcept
        {
            if (&other != this)
            {
                this->id = other.id;
                other.id = NullId;
            }

            return *this;
        }

        [[nodiscard]] bool isNull() const
        {
            return this->id == NullId;
        }

        std::strong_ordering operator<=> (const PointLight&) const = delete;

    private:
        explicit PointLight(u32 id_)
            : id {id_}
        {}
        friend class ChunkManager;

        u32 id;
    };
} // namespace voxel
