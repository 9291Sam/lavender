#pragma once

#include "ecs/raw_entity.hpp"
#include "entity_component_system_manager.hpp"

namespace ecs
{
    struct Entity : public RawEntity
    {};

    class UniqueEntity : public Entity
    {
    public:
        UniqueEntity() noexcept = default;
        explicit UniqueEntity(RawEntity e)
            : Entity {RawEntity {e}}
        {}
        ~UniqueEntity()
        {
            getGlobalECSManager()->destroyEntity(*this);
        }

        UniqueEntity(const UniqueEntity&) = delete;
        UniqueEntity(UniqueEntity&&) noexcept;
        UniqueEntity& operator= (const UniqueEntity&) = delete;
        UniqueEntity& operator= (UniqueEntity&&) noexcept;

        Entity release()
        {
            Entity e = *this;

            this->id = NullEntity;

            return e;
        }

        void reset()
        {
            getGlobalECSManager()->destroyEntity(*this);

            this->id = NullEntity;
        }
    };
} // namespace ecs