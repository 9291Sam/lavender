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

    struct RawEntity
    {
        u32 id = NullEntity;

        template<class C>
        void addComponent(C&&, std::source_location = std::source_location::current()) const;
    };

    template<auto O>
    struct InherentEntityBase
    {
        static constexpr std::size_t offset()
        {
            return O(0);
        }

        [[nodiscard]] RawEntity getId() const
        {
            // NOLINTNEXTLINE;
            return *reinterpret_cast<const RawEntity*>(
                reinterpret_cast<const std::byte*>(this) + offset());
        }

        operator RawEntity () const
        {
            return this->getId();
        }

        template<class C>
        void addComponent(C&& c, std::source_location l = std::source_location::current()) const
        {
            this->getId().addComponent(std::forward<C>(c), l);
        }
    };

    template<typename T, typename U>
    struct dependent
    {
        using type = T;
    };

    template<typename T, typename U>
    using dependent_t = typename dependent<T, U>::type;

#define DERIVE_INHERENT_ENTITY(type, name)                                                         \
    public ::ecs::InherentEntityBase<                                                              \
        [](auto dummy)                                                                             \
        {                                                                                          \
            using T = ::ecs::dependent_t<type, decltype(dummy)>;                                   \
            static_assert(                                                                         \
                std::same_as<::ecs::RawEntity, decltype(T::name)>,                                 \
                "The field pointed to must be an instance of RawEntity");                          \
            return offsetof(T, name);                                                              \
        }>

} // namespace ecs
