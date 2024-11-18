#pragma once

#include "ecs/raw_entity.hpp"
#include "entity_component_system_manager.hpp"

namespace ecs
{
    // This is a bare
    struct Entity : public RawEntity
    {
        void addComponent()
        {
            getGlobalECSManager()->template addComponent<int>(*this, 3);
        }
    };

    class UniqueEntity : public Entity
    {
    public:
        UniqueEntity() noexcept;
        ~UniqueEntity()
        {
            getGlobalECSManager()->destroyEntity(*this);
        }

        UniqueEntity(const UniqueEntity&) = delete;
        UniqueEntity(UniqueEntity&&) noexcept;
        UniqueEntity& operator= (const UniqueEntity&) = delete;
        UniqueEntity& operator= (UniqueEntity&&) noexcept;

        Entity release();
        void   reset();
    };
} // namespace ecs