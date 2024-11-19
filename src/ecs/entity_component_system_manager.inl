#pragma once

#include "ecs/raw_entity.hpp"
#include "entity_component_system_manager.hpp"
#include "util/log.hpp"
#include <ctti/type_id.hpp>
#include <expected>
#include <source_location>
#include <type_traits>

namespace ecs
{

    template<class T, class... Args>
        requires DerivedFromAutoBase<T, InherentEntityBase>
              && std::is_constructible_v<T, RawEntity, Args...>
    T* EntityComponentSystemManager::allocateRawInherentEntity(Args&&... args) const
    {
        // TODO: dont use system allocator
        return ::new T {this->createRawEntity(), std::forward<Args...>(args)...}; // NOLINT
    }

    template<class T>
    void EntityComponentSystemManager::freeRawInherentEntity(T* ptr) const
    {
        return ::delete ptr; // NOLINT
    }

    template<class T, class... Args>
    auto EntityComponentSystemManager::allocateInherentEntity(Args&&... args) const
        -> ManagedEntityPtr<T>
    {
        return ManagedEntityPtr<T> {
            this->allocateRawInherentEntity<T>(std::forward<Args...>(args)...),
            EntityDeleter<T> {}};
    }

    template<class C>
    auto
    EntityComponentSystemManager::tryAddComponent(RawEntity e, C&& c) const -> TryAddComponentResult
    {
        bool added = false;

        const std::size_t visited = this->entities_guard->cvisit(
            e.id,
            [&](const auto&)
            {
                this->accessComponentStorage<C>(
                    [&](boost::unordered::concurrent_flat_map<u32, C>& storage)
                    {
                        added = storage.insert({e.id, std::forward<C>(c)});
                    });
            });

        if (visited == 0)
        {
            return TryAddComponentResult::EntityDead;
        }
        else if (added)
        {
            return TryAddComponentResult::NoError;
        }
        else
        {
            return TryAddComponentResult::ComponentAlreadyPresent;
        }
    }

    template<class C>
    void EntityComponentSystemManager::addComponent(
        RawEntity e, C&& c, std::source_location location) const
    {
        switch (this->tryAddComponent(e, std::forward<C>(c)))
        {
        case TryAddComponentResult::NoError:
            break;
        case TryAddComponentResult::ComponentAlreadyPresent:
            util::logWarn<>("Duplicate insertion of component", location);
            break;
        case TryAddComponentResult::EntityDead:
            util::logWarn<>("Tried to insert component on dead entity", location);
            break;
        }
    }

    template<class C>
    auto EntityComponentSystemManager::tryRemoveComponent(RawEntity e) const
        -> std::expected<C, TryRemoveComponentResult>
    {
        std::optional<C> removedComponent = std::nullopt;

        const std::size_t visited = this->entities_guard->cvisit(
            e.id,
            [&](const auto&)
            {
                this->accessComponentStorage<C>(
                    [&](boost::unordered::concurrent_flat_map<u32, C>& storage)
                    {
                        storage.erase_if(
                            e.id,
                            [&](const std::pair<u32, C>& c)
                            {
                                removedComponent = std::move(c.second);

                                return true;
                            });
                    });
            });

        if (visited == 0)
        {
            return std::unexpected(TryRemoveComponentResult::EntityDead);
        }
        else if (removedComponent.has_value())
        {
            return *std::move(removedComponent);
        }
        else
        {
            return std::unexpected(TryRemoveComponentResult::ComponentNotPresent);
        }
    }
    template<class C, bool PanicOnFail>
    C EntityComponentSystemManager::removeComponent(
        RawEntity e, std::source_location location) const
    {
        static_assert(
            PanicOnFail || std::is_default_constructible_v<C>,
            "If you don't panic on the removal of a component it must be default constructable!");

        const std::expected<C, TryRemoveComponentResult> result = this->tryRemoveComponent<C>(e);

        if (result.has_value())
        {
            return *result;
        }
        else
        {
            if constexpr (PanicOnFail)
            {
                switch (result.error())
                {
                case TryRemoveComponentResult::ComponentNotPresent:
                    util::panic<>("Tried to remove a component which was not present!", location);
                    break;
                case TryRemoveComponentResult::EntityDead:
                    util::panic<>("Tried to remove a component on a dead entity!", location);
                    break;
                }
            }
            else
            {
                switch (result.error())
                {
                case TryRemoveComponentResult::ComponentNotPresent:
                    util::logWarn<>("Tried to remove a component which was not present!", location);
                    break;
                case TryRemoveComponentResult::EntityDead:
                    util::logWarn<>("Tried to remove a component on a dead entity!", location);
                    break;
                }

                return C {};
            }
        }
    }

    template<class C>
    auto EntityComponentSystemManager::tryMutateComponent(
        RawEntity e, std::invocable<C&> auto func) const -> TryPeerComponentResult
    {
        std::size_t componentExists = 0;

        const std::size_t visited = this->entities_guard->cvisit(
            e.id,
            [&](const auto&)
            {
                this->accessComponentStorage<C>(
                    [&](boost::unordered::concurrent_flat_map<u32, C>& storage)
                    {
                        componentExists = storage.visit(func);
                    });
            });

        if (visited == 0)
        {
            return TryPeerComponentResult::EntityDead;
        }
        else if (componentExists != 0)
        {
            return TryPeerComponentResult::NoError;
        }
        else
        {
            return TryPeerComponentResult::ComponentNotPresent;
        }
    }

    template<class C>
    void EntityComponentSystemManager::mutateComponent(
        RawEntity e, std::invocable<C&> auto func, std::source_location location) const
    {
        switch (this->tryMutateComponent<C>(e, func))
        {
        case TryPeerComponentResult::EntityDead:
            util::logWarn<>("Tried to mutate a component on a dead entity!", location);
            break;
        case TryPeerComponentResult::NoError:
            break;
        case TryPeerComponentResult::ComponentNotPresent:
            util::logWarn<>("Tried to mutate a component on an entity which lacked it!", location);
            break;
        }
    }

    template<class C>
    auto EntityComponentSystemManager::tryReadComponent(
        RawEntity e, std::invocable<const C&> auto func) const -> TryPeerComponentResult
    {
        std::size_t componentExists = 0;

        const std::size_t visited = this->entities_guard->cvisit(
            e.id,
            [&](const auto&)
            {
                this->accessComponentStorage<C>(
                    [&](boost::unordered::concurrent_flat_map<u32, C>& storage)
                    {
                        componentExists = storage.cvisit(func);
                    });
            });

        if (visited == 0)
        {
            return TryPeerComponentResult::EntityDead;
        }
        else if (componentExists != 0)
        {
            return TryPeerComponentResult::NoError;
        }
        else
        {
            return TryPeerComponentResult::ComponentNotPresent;
        }
    }

    template<class C>
    void EntityComponentSystemManager::readComponent(
        RawEntity e, std::invocable<const C&> auto func, std::source_location location) const
    {
        switch (this->tryReadComponent<C>(e, func))
        {
        case TryPeerComponentResult::EntityDead:
            util::logWarn<>("Tried to mutate a component on a dead entity!", location);
            break;
        case TryPeerComponentResult::NoError:
            break;
        case TryPeerComponentResult::ComponentNotPresent:
            util::logWarn<>("Tried to mutate a component on an entity which lacked it!", location);
            break;
        }
    }

    template<class C>
    auto EntityComponentSystemManager::tryHasComponent(RawEntity e) const -> TryHasComponentResult
    {
        bool componentExists = false;

        const std::size_t visited = this->entities_guard->cvisit(
            e.id,
            [&](const auto&)
            {
                this->accessComponentStorage<C>(
                    [&](boost::unordered::concurrent_flat_map<u32, C>& storage)
                    {
                        componentExists = storage.contains();
                    });
            });

        if (visited == 0)
        {
            return TryHasComponentResult::EntityDead;
        }
        else if (componentExists != 0)
        {
            return TryHasComponentResult::HasComponent;
        }
        else
        {
            return TryHasComponentResult::NoComponent;
        }
    }

    template<class C>
    bool
    EntityComponentSystemManager::hasComponent(RawEntity e, std::source_location location) const
    {
        switch (this->tryHasComponent<C>(e))
        {
        case TryHasComponentResult::EntityDead:
            util::logWarn<>("Tried to pool component existence on a dead entity!", location);
            return false;
        case TryHasComponentResult::HasComponent:
            return true;
        case TryHasComponentResult::NoComponent:
            return false;
        }
    }

    template<class C>
    void EntityComponentSystemManager::accessComponentStorage(
        std::invocable<boost::unordered::concurrent_flat_map<u32, C>&> auto func) const
    {
        const bool needsCreation = this->component_storage.readLock(
            [&](const std::unordered_map<ctti::type_id_t, std::unique_ptr<ComponentStorageBase>>&
                    erasedStorage)
            {
                auto it = erasedStorage.find(ctti::type_id<C>());

                if (it != erasedStorage.cend())
                {
                    func(*reinterpret_cast<boost::unordered::concurrent_flat_map<u32, C>*>(
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
                    func(*reinterpret_cast<boost::unordered::concurrent_flat_map<u32, int>*>(
                        erasedStorage
                            .insert({ctti::type_id<C>(), std::make_unique<ComponentStorage<C>>()})
                            .first->second->getRawStorage()));
                });
        }
    }

    template<class C>
    void RawEntity::addComponent(C&& c, std::source_location location) const
    {
        ecs::getGlobalECSManager()->addComponent(*this, std::forward<C>(c), location);
    }

} // namespace ecs
