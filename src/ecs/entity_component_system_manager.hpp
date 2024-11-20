#pragma once

#include "ecs/component_storage.hpp"
#include "ecs/raw_entity.hpp"
#include "raw_entity.hpp"
#include "util/log.hpp"
#include "util/threads.hpp"
#include <atomic>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <boost/unordered/concurrent_flat_set.hpp>
#include <concepts>
#include <ctti/type_id.hpp>
#include <expected>
#include <memory>
#include <source_location>
#include <type_traits>
#include <unordered_map>

namespace ecs
{
    class UniqueEntity;

    // I have absolutely no idea, thank you @melak47 NOLINT
    template<typename Derived, template<auto> class BaseTemplate>
    struct IsDerivedFromAutoBase
    {
    private:
        template<auto Arg>
        static std::true_type test(BaseTemplate<Arg>*);

        static std::false_type test(...);

    public:
        static constexpr bool Value = decltype(test(std::declval<Derived*>()))::value;
    };

    template<typename T, template<auto> class BaseTemplate>
    concept DerivedFromAutoBase = IsDerivedFromAutoBase<T, BaseTemplate>::Value;

    enum class TryAddComponentResult
    {
        NoError,
        ComponentAlreadyPresent,
        EntityDead,
    };

    enum class TryRemoveComponentResult
    {
        ComponentNotPresent,
        EntityDead
    };

    enum class TryPeerComponentResult
    {
        NoError,
        ComponentNotPresent,
        EntityDead
    };

    enum class TryHasComponentResult
    {
        NoComponent,
        HasComponent,
        EntityDead
    };

    enum class TryGetComponentResult
    {
        ComponentNotPresent,
        EntityDead,
    };

    enum class TryReplaceComponentResult
    {
        ComponentNotPresent,
        EntityDead,
    };

    enum class TrySetOrInsertComponentResult
    {
        NoError,
        EntityDead
    };

    class EntityComponentSystemManager
    {
    public:

        template<typename T>
        struct EntityDeleter
        {
            void operator() (T* entity) const
            {
                ecs::getGlobalECSManager()->freeRawInherentEntity(entity);
            }
        };

        template<typename T>
        using ManagedEntityPtr = std::unique_ptr<T, EntityDeleter<T>>;

        EntityComponentSystemManager()
            : entities_guard {std::make_unique<boost::unordered::concurrent_flat_set<u32>>()}
            , component_storage {{}}
        {}

        ~EntityComponentSystemManager()
        {
            const std::size_t retainedEntities = this->entities_guard->size();

            util::assertWarn(retainedEntities == 0, "Retained {} entities", retainedEntities);
        }

        EntityComponentSystemManager(const EntityComponentSystemManager&)             = delete;
        EntityComponentSystemManager(EntityComponentSystemManager&&)                  = delete;
        EntityComponentSystemManager& operator= (const EntityComponentSystemManager&) = delete;
        EntityComponentSystemManager& operator= (EntityComponentSystemManager&&)      = delete;

        template<class T, class... Args>
            requires DerivedFromAutoBase<T, InherentEntityBase>
                  && std::is_constructible_v<T, UniqueEntity, Args...>
        [[nodiscard]] T* allocateRawInherentEntity(Args&&...) const;
        template<class T>
        void freeRawInherentEntity(T*) const;

        template<class T, class... Args>
        ManagedEntityPtr<T> allocateInherentEntity(Args&&...) const;

        [[nodiscard]] UniqueEntity createEntity() const;
        [[nodiscard]] RawEntity    createRawEntity() const;
        [[nodiscard]] bool         tryDestroyEntity(RawEntity) const;
        void destroyEntity(RawEntity, std::source_location = std::source_location::current()) const;

        [[nodiscard]] bool isEntityAlive(RawEntity) const;

        template<class C>
        [[nodiscard]] TryAddComponentResult tryAddComponent(RawEntity, C) const;
        template<class C>
        void addComponent(
            RawEntity, C, std::source_location = std::source_location::current()) const;

        template<class C>
        [[nodiscard]] std::expected<C, TryRemoveComponentResult>
            tryRemoveComponent(RawEntity) const;
        template<class C, bool PanicOnFail = false>
        C removeComponent(RawEntity, std::source_location = std::source_location::current()) const;

        template<class C>
        [[nodiscard]] TryPeerComponentResult
            tryMutateComponent(RawEntity, std::invocable<C&> auto) const;
        template<class C>
        void mutateComponent(
            RawEntity,
            std::invocable<C&> auto,
            std::source_location = std::source_location::current()) const;

        template<class C>
        [[nodiscard]] TryPeerComponentResult
            tryReadComponent(RawEntity, std::invocable<const C&> auto) const;
        template<class C>
        void readComponent(
            RawEntity,
            std::invocable<const C&> auto,
            std::source_location = std::source_location::current()) const;

        template<class C>
        [[nodiscard]] TryHasComponentResult tryHasComponent(RawEntity) const;
        template<class C>
        [[nodiscard]] bool
            hasComponent(RawEntity, std::source_location = std::source_location::current()) const;

        template<class C>
            requires std::is_copy_constructible_v<C>
        [[nodiscard]] std::expected<C, TryGetComponentResult> tryGetComponent(RawEntity) const;
        template<class C, bool PanicOnFail = false>
            requires std::is_copy_constructible_v<C>
        [[nodiscard]] C
            getComponent(RawEntity, std::source_location = std::source_location::current()) const;

        template<class C>
        [[nodiscard]] TryPeerComponentResult trySetComponent(RawEntity, C) const;
        template<class C>
        void setComponent(
            RawEntity, C, std::source_location = std::source_location::current()) const;

        template<class C>
            requires std::is_copy_constructible_v<C>
        [[nodiscard]] std::expected<C, TryReplaceComponentResult>
            tryReplaceComponent(RawEntity, C) const;
        template<class C, bool PanicOnFail = false>
            requires std::is_copy_constructible_v<C>
        C replaceComponent(
            RawEntity, C, std::source_location = std::source_location::current()) const;

        template<class C>
        [[nodiscard]] TrySetOrInsertComponentResult trySetOrInsertComponent(RawEntity, C) const;
        template<class C>
        void setOrInsertComponent(
            RawEntity, C, std::source_location = std::source_location::current()) const;

        template<class C>
        void iterateComponents(std::invocable<RawEntity, C&> auto) const;
    private:

        [[nodiscard]] u32 getNewEntityId() const
        {
            return this->next_entity_id.fetch_add(1, std::memory_order_relaxed);
        }

        template<class C>
        void accessComponentStorage(
            std::invocable<boost::unordered::concurrent_flat_map<u32, C>&> auto func) const;

        mutable std::atomic<u32> next_entity_id;

        std::unique_ptr<boost::unordered::concurrent_flat_set<u32>> entities_guard;
        util::RwLock<std::unordered_map<ctti::type_id_t, std::unique_ptr<ComponentStorageBase>>>
            component_storage;
    };

    template<class T>
    using ManagedEntityPtr = EntityComponentSystemManager::ManagedEntityPtr<T>;

    template<class T, class... Args>
    ManagedEntityPtr<T> allocateInherentEntity(Args&&... args)
    {
        return ecs::getGlobalECSManager()->allocateInherentEntity<T>(
            std::forward<Args...>(args)...);
    }

    [[nodiscard]] UniqueEntity createEntity();
} // namespace ecs

#include "entity_component_system_manager.inl"