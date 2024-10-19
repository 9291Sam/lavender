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

    class PointLight
    {
    public:
        static constexpr u32 NullChunk = static_cast<u32>(-1);
    public:
        PointLight()
            : chunk {NullChunk}
            , data {}
        {}
        ~PointLight()
        {
            if (this->chunk != NullChunk)
            {
                util::logWarn("Leaked PointLight!");
            }
        }

        PointLight(const PointLight&) = delete;
        PointLight(PointLight&& other) noexcept
            : chunk {other.chunk}
            , data {other.data}
        {
            other.chunk = NullChunk;
            other.data  = {};
        }
        PointLight& operator= (const PointLight&) = delete;
        PointLight& operator= (PointLight&& other) noexcept
        {
            if (&other != this)
            {
                this->chunk = other.chunk;
                other.chunk = NullChunk;

                this->data = other.data;
                other.data = {};
            }

            return *this;
        }

        [[nodiscard]] bool isNull() const
        {
            return this->chunk == NullChunk;
        }

        std::strong_ordering operator<=> (const PointLight&) const = delete;

    private:
        explicit PointLight(u32 chunk_, InternalPointLight data_)
            : chunk {chunk_}
            , data {data_}
        {}
        friend class ChunkManager;

        u32                chunk;
        InternalPointLight data;
    };
} // namespace voxel
