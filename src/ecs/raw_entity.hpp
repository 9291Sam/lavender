#pragma once

#include "util/misc.hpp"
#include <limits>

namespace ecs
{
    class EntityComponentSystemManager;

    const EntityComponentSystemManager* getGlobalECSManager();

    void installGlobalECSManagerRacy();
    void removeGlobalECSManagerRacy();

    static constexpr u32 NullEntity = std::numeric_limits<u32>::max();

    template<auto O>
    struct RawEntityBase
    {
        static constexpr std::size_t offset()
        {
            return O(0);
        }

        [[nodiscard]] u32 getId() const
        {
            // NOLINTNEXTLINE;
            return *reinterpret_cast<const u32*>(
                reinterpret_cast<const std::byte*>(this) + offset());
        }

        template<class C>
        void addComponent(C&&, std::source_location = std::source_location::current()) const;
    };

    template<typename T, typename U>
    struct dependent
    {
        using type = T;
    };

    template<typename T, typename U>
    using dependent_t = typename dependent<T, U>::type;

#define DERIVE_ENTITY(type, name)                                                                  \
    public ::ecs::RawEntityBase<[](auto dummy)                                                     \
                                {                                                                  \
                                    using T = ::ecs::dependent_t<type, decltype(dummy)>;           \
                                    static_assert(std::same_as<u32, decltype(T::name)>);           \
                                    return offsetof(T, name);                                      \
                                }>

    struct RawEntity : DERIVE_ENTITY(RawEntity, id)
    {
        u32 id = NullEntity;
    };

} // namespace ecs
