#include "entity_component_system_manager.hpp"

namespace ecs
{

    RawEntity EntityComponentSystemManager::createEntity() const
    {
        while (true)
        {
            const u32 newId = this->getNewEntityId();

            if (newId == NullEntity || this->entities_guard->contains(newId))
            {
                util::logWarn("Duplicate allocation of entity {}", newId);
                continue;
            }

            this->entities_guard->insert({newId});

            return RawEntity {.id {newId}};
        }
    }

    bool EntityComponentSystemManager::tryDestroyEntity(RawEntity e) const
    {
        const std::size_t visited = this->entities_guard->visit(
            e.id,
            [this](const auto& entityId)
            {
                this->component_storage.readLock(
                    [&](const auto& s)
                    {
                        for (const auto& [id, storage] : s)
                        {
                            storage->clearAllComponentsForId(entityId);
                        }
                    });
            });

        return visited != 0;
    }

    void
    EntityComponentSystemManager::destroyEntity(RawEntity e, std::source_location location) const
    {
        util::assertWarn<>(
            this->tryDestroyEntity(e), "Tried to destroy an already dead entity!", location);
    }

    bool EntityComponentSystemManager::isEntityAlive(RawEntity e) const
    {
        return this->entities_guard->contains(e.id);
    }

} // namespace ecs