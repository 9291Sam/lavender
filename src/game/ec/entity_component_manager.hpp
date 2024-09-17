#pragma once

#include "component_storage.hpp"
#include "components.hpp"
#include "entity.hpp"
#include "game/ec/entity_storage.hpp"
#include "util/threads.hpp"
#include <__expected/expected.h>
#include <array>
#include <boost/container/small_vector.hpp>
#include <boost/unordered/concurrent_flat_set.hpp>
#include <cstddef>
#include <source_location>
#include <type_traits>
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
        tryAddComponent(Entity e, C c) const
        {
            return this->component_storage[C::Id].lock(
                [&](MuckedComponentStorage& componentStorage)
                {
                    const U32 storedOffset =
                        componentStorage.insertComponent(e, c);

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
            C                    c,
            std::source_location location =
                std::source_location::current()) const
        {
            std::expected<void, ComponentModificationError> tryAddResult =
                this->tryAddComponent(e, c);

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
                }
            }
        }

        // template<Component C>
        // [[nodiscard]] std::optional<C> tryRemoveComponent(Entity) const;
        // template<Component C>
        // C removeComponent(Entity) const;

        // template<Component... C>
        // [[nodiscard]] bool tryModifyComponent(std::invocable<C...> auto)
        // const; template<Component... C> void
        // modifyComponent(std::invocable<C...> auto) const;

        // template<Component C>
        // [[nodiscard]] bool hasComponent(Entity) const;

        // bool tryAddComponent

        // Returns true if the entity was destroyed, otherwise the entity wasn't
        // alive
        // bool tryDestroyEntity(Entity e) const
        // {
        //     return this->entity_storage.lock(
        //         [&](EntityStorage& storage)
        //         {
        //             if (!storage.isEntityAlive(e))
        //             {
        //                 return false;
        //             }

        //             storage.deleteEntity(e);

        //             return true;
        //         });
        // }

        template<Component C>
        void iterateComponents(std::invocable<Entity, const C&> auto func) const
        {
            this->component_storage[C::Id].lock(
                [&](MuckedComponentStorage& storage)
                {
                    storage.iterateComponents<C>(func);
                });
        }

    private:

        std::array<
            util::RecursiveMutex<MuckedComponentStorage>,
            NumberOfGameComponents>
                                            component_storage;
        util::RecursiveMutex<EntityStorage> entity_storage;
    };

} // namespace game::ec
