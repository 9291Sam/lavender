#include "raw_entity.hpp"
#include "entity_component_system_manager.hpp"
#include <utility>

namespace ecs
{
    static EntityComponentSystemManager* globalECSManager = nullptr; // NOLINT

    const EntityComponentSystemManager* getGlobalECSManager()
    {
        return globalECSManager;
    }

    void installGlobalECSManagerRacy()
    {
        globalECSManager = new EntityComponentSystemManager {}; // NOLINT
    }

    void removeGlobalECSManagerRacy()
    {
        const EntityComponentSystemManager* ecsManager = std::exchange(globalECSManager, nullptr);

        delete ecsManager; // NOLINT
    }
} // namespace ecs