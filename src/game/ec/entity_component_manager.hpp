#pragma once

#include "components.hpp"
#include "entity.hpp"
#include "game/ec/entity_storage.hpp"
#include "type_erased_component_storage.hpp"
#include "util/log.hpp"
#include "util/threads.hpp"
#include <array>
#include <boost/container/small_vector.hpp>
#include <boost/unordered/concurrent_flat_set.hpp>
#include <expected>
#include <source_location>
#include <util/misc.hpp>

namespace game::ec
{
    struct BarComponent : ComponentBase<BarComponent>
    {};

    struct FooComponent : ComponentBase<FooComponent>
    {
        U8 a;
    };

    static_assert(FooComponent::Id == 0);
    static_assert(BarComponent::Id == 1);

    class EntityComponentManager
    {
    public:

        explicit EntityComponentManager();
        ~EntityComponentManager() noexcept = default;

        EntityComponentManager(const EntityComponentManager&) = delete;
        EntityComponentManager(EntityComponentManager&&)      = delete;
        EntityComponentManager&
        operator= (const EntityComponentManager&)                    = delete;
        EntityComponentManager& operator= (EntityComponentManager&&) = delete;

        /// Creates a new entity without any components attached
        [[nodiscard]] Entity createEntity() const;

        /// Tries to destroy a given entity
        /// Returns:
        ///    true - the entity was successfully destroyed
        ///    false - the entity was already destroyed
        [[nodiscard]] bool tryDestroyEntity(Entity) const;

        /// Destroys the given entity, warns on failure
        void destroyEntity(
            Entity,
            std::source_location = std::source_location::current()) const;

        // /// Returns whether or not this entity is alive
        bool isEntityAlive(Entity) const;

        template<Component C>
        [[nodiscard]] std::expected<void, ComponentModificationError>
        tryAddComponent(Entity e, C&& c) const
        {
            return this->component_storage[C::Id].lock(
                [&](TypeErasedComponentStorage& componentStorage)
                {
                    const U32 storedOffset =
                        componentStorage.addComponent(e, std::forward<C>(c));

                    std::expected<void, ComponentModificationError>
                        wasComponentAddedToEntity = this->entity_storage.lock(
                            [&](EntityStorage& entityStorage)
                            {
                                return entityStorage.addComponent(
                                    e,
                                    EntityStorage::EntityComponentStorage {
                                        .component_type_id {C::Id},
                                        .component_storage_offset {
                                            storedOffset}});
                            });

                    // if we failed to add the component to the entity for some
                    // reason, delete the component from storage
                    if (!wasComponentAddedToEntity.has_value())
                    {
                        componentStorage.deleteComponent(storedOffset);
                    }

                    return wasComponentAddedToEntity;
                });
        }

        template<Component C>
        void addComponent(
            Entity               e,
            C&&                  c,
            std::source_location location =
                std::source_location::current()) const
        {
            std::expected<void, ComponentModificationError> tryAddResult =
                this->tryAddComponent(e, std::forward<C>(c));

            if (!tryAddResult.has_value())
            {
                switch (tryAddResult.error())
                {
                case ComponentModificationError::ComponentConflict:
                    util::logWarn<std::string_view>(
                        "Tried to add multiple components of type {} to entity",
                        getComponentName<C::Id>(),
                        location);
                    break;
                case ComponentModificationError::EntityDead:
                    util::logWarn<std::string_view>(
                        "Tried to add component of type {} to dead entity",
                        getComponentName<C::Id>(),
                        location);
                    break;
                default:
                    util::panic(
                        "Unexpected tryAddResult.error() {}",
                        util::toUnderlying(tryAddResult.error()));
                }
            }
        }

        template<Component C>
        [[nodiscard]] std::expected<std::optional<C>, EntityDead>
        tryRemoveComponent(Entity e) const
        {
            return this->entity_storage.lock(
                [&](EntityStorage& entityStorage)
                {
                    std::expected<
                        EntityStorage::EntityComponentStorage,
                        ComponentModificationError>
                        result = entityStorage.removeComponent(e, C::Id);

                    if (result.has_value())
                    {
                        return this->component_storage[C::Id].lock(
                            [&](TypeErasedComponentStorage& componentStorage)
                            {
                                return componentStorage.takeComponent<C>(
                                    result->component_storage_offset);
                            });
                    }
                    else
                    {
                        switch (result.error())
                        {
                        case ComponentModificationError::ComponentConflict:
                            return std::expected<std::optional<C>, EntityDead> {
                                std::nullopt};
                        case ComponentModificationError::EntityDead:
                            return std::unexpected(EntityDead {});
                        default:
                            util::panic(
                                "Unexpected tryAddResult.error() {}",
                                util::toUnderlying(result.error()));
                        }
                    }
                });
        }
        template<Component C>
        C removeComponent(Entity e) const
        {
            std::expected<std::optional<C>, EntityDead> tryRemoveResult =
                this->tryRemoveComponent<C>(e);

            if (!tryRemoveResult.has_value())
            {
                util::panic("tried to remove component from dead entity!");
            }

            if (!tryRemoveResult->has_value())
            {
                util::panic(
                    "tried to remove component {} which didn't exist",
                    getComponentName<C::Id>());
            }

            return std::move(**tryRemoveResult);
        }

        // template<Component... C>
        // [[nodiscard]] bool tryModifyComponent(std::invocable<C...> auto)
        // const; template<Component... C> void
        // modifyComponent(std::invocable<C...> auto) const;

        template<Component C>
        [[nodiscard]] bool hasComponent(Entity e) const
        {
            return this->entity_storage.lock(
                [&](EntityStorage& storage)
                {
                    return storage.hasComponent(e, C::Id);
                });
        }

        template<Component C>
        void iterateComponents(std::invocable<Entity, const C&> auto func) const
        {
            this->component_storage[C::Id].lock(
                [&](TypeErasedComponentStorage& storage)
                {
                    storage.iterateComponents<C>(func);
                });
        }

    private:

        std::array<
            util::RecursiveMutex<TypeErasedComponentStorage>,
            NumberOfGameComponents>
                                            component_storage;
        util::RecursiveMutex<EntityStorage> entity_storage;
    };

} // namespace game::ec
