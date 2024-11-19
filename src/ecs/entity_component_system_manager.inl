#pragma once

#include "ecs/raw_entity.hpp"
#include "entity_component_system_manager.hpp"
#include "util/log.hpp"
#include "util/misc.hpp"
#include <ctti/type_id.hpp>
#include <expected>
#include <optional>
#include <source_location>
#include <type_traits>

namespace ecs
{

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
                        componentExists = storage.cvisit(
                            e.id,
                            [&](const std::pair<u32, C>& pair)
                            {
                                func(pair.second);
                            });
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
            util::logWarn<>("Tried to poll component existence on a dead entity!", location);
            return false;
        case TryHasComponentResult::HasComponent:
            return true;
        case TryHasComponentResult::NoComponent:
            return false;
        }
    }

    template<class C>
        requires std::is_copy_constructible_v<C>
    [[nodiscard]] auto EntityComponentSystemManager::tryGetComponent(RawEntity e) const
        -> std::expected<C, TryGetComponentResult>
    {
        std::optional<C> maybeC = std::nullopt;

        const TryPeerComponentResult result = this->tryReadComponent<C>(
            e,
            [&](const C& c)
            {
                maybeC = c;
            });

        if (maybeC.has_value())
        {
            return *maybeC;
        }
        else
        {
            switch (result)
            {
            case TryPeerComponentResult::NoError:
                util::assertFatal(false, "Unreachable");
                util::debugBreak();
            case TryPeerComponentResult::ComponentNotPresent:
                return std::unexpected(TryGetComponentResult::ComponentNotPresent);
            case TryPeerComponentResult::EntityDead:
                return std::unexpected(TryGetComponentResult::EntityDead);
            }
        }
    }

    template<class C, bool PanicOnFail>
        requires std::is_copy_constructible_v<C>
    [[nodiscard]] C
    EntityComponentSystemManager::getComponent(RawEntity e, std::source_location location) const
    {
        std::expected<C, TryGetComponentResult> result = this->tryGetComponent<C>(e);

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
                case TryGetComponentResult::ComponentNotPresent:
                    util::panic<>("Tried to get a component which was not present!", location);
                    break;
                case TryGetComponentResult::EntityDead:
                    util::panic<>("Tried to get a component on a dead entity!", location);
                    break;
                }
            }
            else
            {
                switch (result.error())
                {
                case TryGetComponentResult::ComponentNotPresent:
                    util::logWarn<>("Tried to get a component which was not present!", location);
                    break;
                case TryGetComponentResult::EntityDead:
                    util::logWarn<>("Tried to get a component on a dead entity!", location);
                    break;
                }

                return C {};
            }
        }
    }

    template<class C>
    [[nodiscard]] auto EntityComponentSystemManager::trySetComponent(RawEntity e, C&& cNew) const
        -> TryPeerComponentResult
    {
        return this->tryMutateComponent<C>(
            e,
            [&](C& cOld)
            {
                cOld = std::forward<C>(cNew);
            });
    }
    template<class C>
    void EntityComponentSystemManager::setComponent(
        RawEntity e, C&& c, std::source_location location) const
    {
        switch (this->trySetComponent<C>(e, std::forward<C>(c)))
        {
        case TryPeerComponentResult::EntityDead:
            util::logWarn<>("Tried to set component on a dead entity!", location);
            return;
        case TryPeerComponentResult::NoError:
            return;
        case TryPeerComponentResult::ComponentNotPresent:
            util::logWarn<>("Tried to set component on an entity which lacked it!", location);
            return;
        }
    }

    template<class C>
        requires std::is_copy_constructible_v<C>
    [[nodiscard]] auto EntityComponentSystemManager::tryReplaceComponent(
        RawEntity e, C&& cNew) const -> std::expected<C, TryReplaceComponentResult>
    {
        std::optional<C> removedComponent = std::nullopt;

        const TryPeerComponentResult result = this->tryMutateComponent<C>(
            e,
            [&](C& cOld)
            {
                removedComponent = std::exchange(cOld, std::forward<C>(cNew));
            });

        if (removedComponent.has_value())
        {
            return *removedComponent;
        }
        else
        {
            switch (result)
            {
            case TryPeerComponentResult::NoError:
                util::assertFatal(false, "unreachable");
                util::debugBreak();
            case TryPeerComponentResult::ComponentNotPresent:
                return std::unexpected(TryReplaceComponentResult::ComponentNotPresent);
            case TryPeerComponentResult::NoError:
                return std::unexpected(TryReplaceComponentResult::EntityDead);
            }
        }
    }
    template<class C, bool PanicOnFail>
        requires std::is_copy_constructible_v<C>
    C EntityComponentSystemManager::replaceComponent(
        RawEntity e, C&& c, std::source_location location) const
    {
        std::expected<C, TryReplaceComponentResult> result =
            this->tryReplaceComponent<C>(e, std::forward<C>(c));

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
                case TryReplaceComponentResult::ComponentNotPresent:
                    util::panic<>("Tried to replace a component which was not present!", location);
                    break;
                case TryReplaceComponentResult::EntityDead:
                    util::panic<>("Tried to replace a component on a dead entity!", location);
                    break;
                }
            }
            else
            {
                switch (result.error())
                {
                case TryReplaceComponentResult::ComponentNotPresent:
                    util::logWarn<>(
                        "Tried to replace a component which was not present!", location);
                    break;
                case TryReplaceComponentResult::EntityDead:
                    util::logWarn<>("Tried to replace a component on a dead entity!", location);
                    break;
                }

                return std::forward<C>(c);
            }
        }
    }

    template<class C>
    [[nodiscard]] auto EntityComponentSystemManager::trySetOrInsertComponent(
        RawEntity e, C&& cNew) const -> TrySetOrInsertComponentResult
    {
        const std::size_t visited = this->entities_guard->cvisit(
            e.id,
            [&](const auto&)
            {
                this->accessComponentStorage<C>(
                    [&](boost::unordered::concurrent_flat_map<u32, C>& storage)
                    {
                        storage.insert_or_assign(e.id, std::forward<C>(cNew));
                    });
            });

        if (visited == 0)
        {
            return TrySetOrInsertComponentResult::EntityDead;
        }
        else
        {
            return TrySetOrInsertComponentResult::NoError;
        }
    }

    template<class C>
    void EntityComponentSystemManager::setOrInsertComponent(
        RawEntity e, C&& c, std::source_location location) const
    {
        const TrySetOrInsertComponentResult result =
            this->trySetOrInsertComponent<C>(e, std::forward<C>(c));

        switch (result)
        {
        case TrySetOrInsertComponentResult::NoError:
            return;
        case TrySetOrInsertComponentResult::EntityDead:
            util::logWarn<>("Tried to set or insert a component on a dead entity!", location);
            return;
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
                    func(*reinterpret_cast<boost::unordered::concurrent_flat_map<u32, C>*>(
                        erasedStorage
                            .insert({ctti::type_id<C>(), std::make_unique<ComponentStorage<C>>()})
                            .first->second->getRawStorage()));
                });
        }
    }

    template<class Concrete>
    [[nodiscard]] bool EntityComponentOperationsCRTPBase<Concrete>::tryDestroyEntity()
    {
        RawEntity& self = this->getRawEntityRefCRTP();

        const bool result = ecs::getGlobalECSManager()->tryDestroyEntity(self);

        self.id = NullEntity;

        return result;
    }
    template<class Concrete>
    void EntityComponentOperationsCRTPBase<Concrete>::destroyEntity(std::source_location location)
    {
        RawEntity& self = this->getRawEntityRefCRTP();

        ecs::getGlobalECSManager()->destroyEntity(self, location);

        self.id = NullEntity;
    }

    template<class Concrete>
    template<class C>
    [[nodiscard]] TryAddComponentResult
    EntityComponentOperationsCRTPBase<Concrete>::tryAddComponent(C&& c) const
    {
        return ecs::getGlobalECSManager()->tryAddComponent(*this, std::forward<C>(c));
    }

    template<class Concrete>
    template<class C>
    void EntityComponentOperationsCRTPBase<Concrete>::addComponent(
        C&& c, std::source_location location) const
    {
        ecs::getGlobalECSManager()->addComponent(*this, std::forward<C>(c), location);
    }

    template<class Concrete>
    template<class C>
    [[nodiscard]] std::expected<C, TryRemoveComponentResult>
    EntityComponentOperationsCRTPBase<Concrete>::tryRemoveComponent() const
    {
        return ecs::getGlobalECSManager()->tryRemoveComponent<C>(*this);
    }

    template<class Concrete>
    template<class C, bool PanicOnFail>
    C EntityComponentOperationsCRTPBase<Concrete>::removeComponent(
        std::source_location location) const
    {
        return ecs::getGlobalECSManager()->removeComponent<C, PanicOnFail>(*this, location);
    }

    template<class Concrete>
    template<class C>
    [[nodiscard]] TryPeerComponentResult
    EntityComponentOperationsCRTPBase<Concrete>::tryMutateComponent(
        std::invocable<C&> auto func) const
    {
        return ecs::getGlobalECSManager()->tryMutateComponent<C>(*this, func);
    }

    template<class Concrete>
    template<class C>
    void EntityComponentOperationsCRTPBase<Concrete>::mutateComponent(
        std::invocable<C&> auto func, std::source_location location) const
    {
        ecs::getGlobalECSManager()->mutateComponent<C>(*this, func, location);
    }

    template<class Concrete>
    template<class C>
    [[nodiscard]] TryPeerComponentResult
    EntityComponentOperationsCRTPBase<Concrete>::tryReadComponent(
        std::invocable<const C&> auto func) const
    {
        return ecs::getGlobalECSManager()->tryReadComponent<C>(*this, func);
    }

    template<class Concrete>
    template<class C>
    void EntityComponentOperationsCRTPBase<Concrete>::readComponent(
        std::invocable<const C&> auto func, std::source_location location) const
    {
        ecs::getGlobalECSManager()->readComponent<C>(*this, func, location);
    }

    template<class Concrete>
    template<class C>
    [[nodiscard]] TryHasComponentResult
    EntityComponentOperationsCRTPBase<Concrete>::tryHasComponent() const
    {
        return ecs::getGlobalECSManager()->tryHasComponent<C>(*this);
    }

    template<class Concrete>
    template<class C>
    [[nodiscard]] bool
    EntityComponentOperationsCRTPBase<Concrete>::hasComponent(std::source_location location) const
    {
        return ecs::getGlobalECSManager()->hasComponent<C>(*this, location);
    }

    template<class Concrete>
    template<class C>
        requires std::is_copy_constructible_v<C>
    [[nodiscard]] std::expected<C, TryGetComponentResult>
    EntityComponentOperationsCRTPBase<Concrete>::tryGetComponent() const
    {
        return ecs::getGlobalECSManager()->tryGetComponent<C>(*this);
    }

    template<class Concrete>
    template<class C, bool PanicOnFail>
        requires std::is_copy_constructible_v<C>
    [[nodiscard]] C
    EntityComponentOperationsCRTPBase<Concrete>::getComponent(std::source_location location) const
    {
        return ecs::getGlobalECSManager()->getComponent<C, PanicOnFail>(*this, location);
    }

    template<class Concrete>
    template<class C>
    [[nodiscard]] TryPeerComponentResult
    EntityComponentOperationsCRTPBase<Concrete>::trySetComponent(C&& c) const
    {
        return ecs::getGlobalECSManager()->trySetComponent(*this, std::forward<C>(c));
    }

    template<class Concrete>
    template<class C>
    void EntityComponentOperationsCRTPBase<Concrete>::setComponent(
        C&& c, std::source_location location) const
    {
        ecs::getGlobalECSManager()->setComponent(*this, std::forward<C>(c), location);
    }

    template<class Concrete>
    template<class C>
        requires std::is_copy_constructible_v<C>
    [[nodiscard]] std::expected<C, TryReplaceComponentResult>
    EntityComponentOperationsCRTPBase<Concrete>::tryReplaceComponent(C&& c) const
    {
        return ecs::getGlobalECSManager()->tryReplaceComponent(*this, std::forward<C>(c));
    }

    template<class Concrete>
    template<class C, bool PanicOnFail>
        requires std::is_copy_constructible_v<C>
    C EntityComponentOperationsCRTPBase<Concrete>::replaceComponent(
        C&& c, std::source_location location) const
    {
        return ecs::getGlobalECSManager()->replaceComponent<C, PanicOnFail>(
            *this, std::forward<C>(c), location);
    }

    template<class Concrete>
    template<class C>
    [[nodiscard]] TrySetOrInsertComponentResult
    EntityComponentOperationsCRTPBase<Concrete>::trySetOrInsertComponent(C&& c) const
    {
        return ecs::getGlobalECSManager()->trySetOrInsertComponent<C>(*this, std::forward<C>(c));
    }

    template<class Concrete>
    template<class C>
    void EntityComponentOperationsCRTPBase<Concrete>::setOrInsertComponent(
        C&& c, std::source_location location) const
    {
        ecs::getGlobalECSManager()->setOrInsertComponent(*this, std::forward<C>(c), location);
    }

} // namespace ecs
