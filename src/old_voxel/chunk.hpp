#pragma once

#include "util/log.hpp"
#include <compare>
#include <util/misc.hpp>

namespace old_voxel
{

    class Chunk
    {
    public:
        static constexpr u32 NullChunk = static_cast<u32>(-1);
    public:
        Chunk()
            : id {NullChunk}
        {}
        ~Chunk()
        {
            if (this->id != NullChunk)
            {
                util::logWarn("Leaked Chunk!");
            }
        }

        Chunk(const Chunk&) = delete;
        Chunk(Chunk&& other) noexcept
            : id {other.id}
        {
            other.id = NullChunk;
        }
        Chunk& operator= (const Chunk&) = delete;
        Chunk& operator= (Chunk&& other) noexcept
        {
            if (&other != this)
            {
                this->id = other.id;
                other.id = NullChunk;
            }

            return *this;
        }

        [[nodiscard]] bool isNull() const
        {
            return this->id == NullChunk;
        }

        std::strong_ordering operator<=> (const Chunk&) const = default;


    private:
        explicit Chunk(u32 id_)
            : id {id_}
        {}
        friend class ChunkManager;

        u32 id;
    };
} // namespace old_voxel
