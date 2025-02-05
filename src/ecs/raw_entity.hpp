#pragma once

#include "util/misc.hpp"
#include <expected>
#include <limits>

namespace ecs
{
    class EntityComponentSystemManager;
    enum class TryAddComponentResult;
    enum class TryRemoveComponentResult;
    enum class TryPeerComponentResult;
    enum class TryHasComponentResult;
    enum class TryGetComponentResult;
    enum class TryReplaceComponentResult;
    enum class TrySetOrInsertComponentResult;

    const EntityComponentSystemManager* getGlobalECSManager();

    void installGlobalECSManagerRacy();
    void removeGlobalECSManagerRacy();

    static constexpr u32 NullEntity = std::numeric_limits<u32>::max();

    struct RawEntity;

    template<class Derived>
    struct EntityComponentOperationsCRTPBase
    {
        [[nodiscard]] RawEntity  getRawEntityCRTP() const;
        [[nodiscard]] RawEntity& getRawEntityRefCRTP();

        operator RawEntity () const; // NOLINT: desired behavior

        [[nodiscard]] bool tryDestroyEntity();
        void               destroyEntity(std::source_location = std::source_location::current());

        template<class C>
        [[nodiscard]] TryAddComponentResult tryAddComponent(C) const;
        template<class C>
        void addComponent(C, std::source_location = std::source_location::current()) const;

        template<class C>
        [[nodiscard]] std::expected<C, TryRemoveComponentResult> tryRemoveComponent() const;
        template<class C, bool PanicOnFail = false>
        C removeComponent(std::source_location = std::source_location::current()) const;

        template<class C>
        [[nodiscard]] TryPeerComponentResult tryMutateComponent(std::invocable<C&> auto) const;
        template<class C>
        void mutateComponent(
            std::invocable<C&> auto, std::source_location = std::source_location::current()) const;

        template<class C>
        [[nodiscard]] TryPeerComponentResult tryReadComponent(std::invocable<const C&> auto) const;
        template<class C>
        void readComponent(
            std::invocable<const C&> auto,
            std::source_location = std::source_location::current()) const;

        template<class C>
        [[nodiscard]] TryHasComponentResult tryHasComponent() const;
        template<class C>
        [[nodiscard]] bool
            hasComponent(std::source_location = std::source_location::current()) const;

        template<class C>
            requires std::is_copy_constructible_v<C>
        [[nodiscard]] std::expected<C, TryGetComponentResult> tryGetComponent() const;
        template<class C, bool PanicOnFail = false>
            requires std::is_copy_constructible_v<C>
        [[nodiscard]] C getComponent(std::source_location = std::source_location::current()) const;

        template<class C>
        [[nodiscard]] TryPeerComponentResult trySetComponent(C) const;
        template<class C>
        void setComponent(C, std::source_location = std::source_location::current()) const;

        template<class C>
            requires std::is_copy_constructible_v<C>
        [[nodiscard]] std::expected<C, TryReplaceComponentResult> tryReplaceComponent(C) const;
        template<class C, bool PanicOnFail = false>
            requires std::is_copy_constructible_v<C>
        C replaceComponent(C, std::source_location = std::source_location::current()) const;

        template<class C>
        [[nodiscard]] TrySetOrInsertComponentResult trySetOrInsertComponent(C) const;
        template<class C>
        void setOrInsertComponent(C, std::source_location = std::source_location::current()) const;

        template<class C>
        void iterateComponents(std::invocable<RawEntity, C&> auto) const;
    };

    struct RawEntity : public EntityComponentOperationsCRTPBase<RawEntity>
    {
        u32 id = NullEntity;

        RawEntity getRawEntity() const
        {
            return *this;
        }

        [[nodiscard]] RawEntity& getRawEntityRef()
        {
            return *this;
        }
    };

    class UniqueEntity;

    template<class Derived, auto O>
    struct InherentEntityBase
        : public EntityComponentOperationsCRTPBase<InherentEntityBase<Derived, O>>
    {
        static constexpr UniqueEntity Derived::* offset()
        {
            return O(0);
        }
        [[nodiscard]] RawEntity getRawEntity() const
        {
            return *reinterpret_cast<const RawEntity*>(               // NOLINT
                reinterpret_cast<const std::byte*>(this) + offset()); // NOLINT
        }

        [[nodiscard]] RawEntity& getRawEntityRef()
        {
            // NOLINTNEXTLINE
            return *reinterpret_cast<RawEntity*>(reinterpret_cast<std::byte*>(this) + offset());
        }
    };

    template<class Derived>
    RawEntity EntityComponentOperationsCRTPBase<Derived>::getRawEntityCRTP() const
    {
        return static_cast<const Derived*>(this)->getRawEntity();
    }

    template<class Derived>
    RawEntity& EntityComponentOperationsCRTPBase<Derived>::getRawEntityRefCRTP()
    {
        return static_cast<Derived*>(this)->getRawEntityRef();
    }

    template<class Derived>
    EntityComponentOperationsCRTPBase<Derived>::operator RawEntity () const
    {
        return this->getRawEntityCRTP();
    }

    template<typename T, typename U>
    struct Dependent
    {
        using Type = T;
    };

    template<typename T, typename U>
    using DependentT = typename Dependent<T, U>::Type;

// NOLINTNEXTLINE
#define DERIVE_INHERENT_ENTITY(type, name)                                                         \
    public ::ecs::InherentEntityBase<                                                              \
        type,                                                                                      \
        [](auto dummy)                                                                             \
        {                                                                                          \
            using T = ::ecs::DependentT<type, decltype(dummy)>;                                    \
            static_assert(                                                                         \
                std::same_as<::ecs::UniqueEntity, decltype(T::name)>,                              \
                "The field pointed to must be an instance of RawEntity");                          \
            return &T::name;                                                                       \
        }>

} // namespace ecs
