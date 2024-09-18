#include "entity_component_manager.hpp"
#include "game/ec/component_storage.hpp"
#include "game/ec/entity.hpp"
#include "game/ec/entity_storage.hpp"
#include "util/threads.hpp"
#include <source_location>
#include <type_traits>

namespace game::ec
{
    EntityComponentManager::EntityComponentManager()
        : component_storage {
              util::RecursiveMutex {
                  MuckedComponentStorage(128, getComponentSize<0>())},
              util::RecursiveMutex {
                  MuckedComponentStorage(128, getComponentSize<1>())},
              util::RecursiveMutex {
                  MuckedComponentStorage(128, getComponentSize<2>())}}
    {}

    Entity EntityComponentManager::createEntity() const
    {
        return this->entity_storage.lock(
            [](EntityStorage& storage)
            {
                return storage.create();
            });
    }

    bool EntityComponentManager::tryDestroyEntity(Entity e) const
    {
        return this->entity_storage.lock(
            [&](EntityStorage& entityStorage)
            {
                const std::span<const EntityStorage::EntityComponentStorage>
                    maybeAllComponents = entityStorage.getAllComponents(e);

                const bool willEntityBeDestroyed = entityStorage.isAlive(e);

                if (willEntityBeDestroyed)
                {
                    for (const EntityStorage::EntityComponentStorage&
                             storedComponent :
                         entityStorage.getAllComponents(e))
                    {
                        this->component_storage[storedComponent
                                                    .component_type_id]
                            .lock(
                                [&](MuckedComponentStorage& componentStorage)
                                {
                                    componentStorage.deleteComponent(
                                        storedComponent
                                            .component_storage_offset,
                                        componentStorage.getDestructor());
                                });
                    }
                }

                util::assertFatal(
                    willEntityBeDestroyed == entityStorage.destroy(e),
                    "this shouldnt ever ever happen if so the entire component "
                    "system is GONE");

                return willEntityBeDestroyed;
            });
    }

    void EntityComponentManager::destroyEntity(
        Entity e, std::source_location location) const
    {
        util::assertWarn<>(
            this->tryDestroyEntity(e),
            "Tried to destroy already destroyed entity!",
            location);
    }

    bool EntityComponentManager::isEntityAlive(Entity e) const
    {
        return this->entity_storage.lock(
            [&](EntityStorage& storage)
            {
                return storage.isAlive(e);
            });
    }
} // namespace game::ec
