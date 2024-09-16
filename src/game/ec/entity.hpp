#pragma once

#include <bit>
#include <compare>
#include <functional>
#include <util/misc.hpp>

namespace game::ec
{
    struct Entity
    {
        Entity()
            : id {static_cast<U32>(-1)}
            , generation {static_cast<U32>(-1)}
        {}

        [[nodiscard]] bool isNull() const
        {
            return (*this) == Entity {};
        }

        constexpr bool operator== (const Entity&) const = default;
        constexpr std::strong_ordering
        operator<=> (const Entity&) const = default;
    private:
        friend class EntityStorage;
        friend class ECManager;

        U32 id;
        U32 generation;
    };

    // NOLINTNEXTLINE
    inline std::size_t hash_value(const Entity& e)
    {
        std::size_t seed = 823489723458432;

        util::hashCombine(seed, std::bit_cast<U64>(e));

        return seed;
    }
} // namespace game::ec

template<>
struct std::hash<game::ec::Entity>
{
    std::size_t operator() (const game::ec::Entity& e) const noexcept
    {
        return game::ec::hash_value(e);
    }
};