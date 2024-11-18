#pragma once

#include "ecs/component_storage.hpp"
#include "raw_entity.hpp"
#include "util/log.hpp"
#include "util/threads.hpp"
#include <atomic>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <boost/unordered/concurrent_flat_set.hpp>
#include <concepts>
#include <ctti/type_id.hpp>
#include <memory>
#include <source_location>
#include <unordered_map>

namespace ecs
{
    class EntityComponentSystemManager
    {
    public:
        enum class Result
        {
            NoError,
            ComponentConflict,
            EntityDead,
        };

        enum class TryHasComponentResult
        {
            NoComponent,
            HasComponent,
            EntityDead
        };
    public:

        EntityComponentSystemManager()
            : entities_guard {std::make_unique<boost::unordered::concurrent_flat_set<u32>>()}
            , component_storage {{}}
        {}

        [[nodiscard]] RawEntity createEntity() const;
        [[nodiscard]] bool      tryDestroyEntity(RawEntity) const;
        void destroyEntity(RawEntity, std::source_location = std::source_location::current()) const;

        [[nodiscard]] bool isEntityAlive(RawEntity) const;

        template<class C>
        [[nodiscard]] Result tryAddComponent(RawEntity, C&&) const;
        template<class C>
        void
        addComponent(RawEntity, C&&, std::source_location = std::source_location::current()) const;

        template<class C>
        [[nodiscard]] Result tryRemoveComponent(RawEntity, C&&) const;
        template<class C>
        void removeComponent(
            RawEntity, std::source_location = std::source_location::current()) const;

        template<class C>
        [[nodiscard]] Result tryMutateComponent(RawEntity, std::invocable<C&> auto) const;
        template<class C>
        void mutateComponent(
            RawEntity,
            std::invocable<C&> auto,
            std::source_location = std::source_location::current()) const;

        template<class C>
        [[nodiscard]] Result tryReadComponent(RawEntity, std::invocable<const C&> auto) const;
        template<class C>
        void readComponent(
            RawEntity,
            std::invocable<C&> auto,
            std::source_location = std::source_location::current()) const;

        template<class C>
        [[nodiscard]] TryHasComponentResult tryHasComponent(RawEntity) const;
        [[nodiscard]] bool
            hasComponent(RawEntity, std::source_location = std::source_location::current()) const;
    private:

        [[nodiscard]] u32 getNewEntityId() const
        {
            return this->next_entity_id.fetch_add(1, std::memory_order_relaxed);
        }

        template<class C>
        void accessComponentStorage(
            std::invocable<const boost::unordered::concurrent_flat_map<u32, C>&> auto func) const
        {
            const bool needsCreation = this->component_storage.readLock(
                [&](const std::unordered_map<
                    ctti::type_id_t,
                    std::unique_ptr<ComponentStorageBase>>& erasedStorage)
                {
                    auto it = erasedStorage.find(ctti::type_id<C>());

                    if (it != erasedStorage.cend())
                    {
                        func(
                            *reinterpret_cast<const boost::unordered::concurrent_flat_map<u32, C>*>(
                                it->second->getRawStorage()));

                        return false;
                    }
                    else
                    {
                        return true;
                    }
                });

            if (needsCreation)
            {
                this->component_storage.writeLock(
                    [&](std::unordered_map<ctti::type_id_t, std::unique_ptr<ComponentStorageBase>>&
                            erasedStorage)
                    {
                        util::logTrace("{} {}", (void*)(&erasedStorage), erasedStorage.size());

                        erasedStorage.insert({ctti::type_id<C>(), {}});

                        util::logTrace("{} {}", (void*)(&erasedStorage), erasedStorage.size());

                        std::unique_ptr<ComponentStorageBase>& ptr =
                            erasedStorage[ctti::type_id<C>()];

                        void* data = ptr->getRawStorage();

                        // func(*reinterpret_cast<
                        //      const boost::unordered::concurrent_flat_map<u32, int>*>(
                        //     ptr->getRawStorage()));
                    });
            }
        }

        mutable std::atomic<u32> next_entity_id;

        std::unique_ptr<boost::unordered::concurrent_flat_set<u32>> entities_guard;
        util::RwLock<std::unordered_map<ctti::type_id_t, std::unique_ptr<ComponentStorageBase>>>
            component_storage;
    };
} // namespace ecs

#include "entity_component_system_manager.inl"