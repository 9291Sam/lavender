#pragma once

#include "ecs/raw_entity.hpp"
#include "entity_component_system_manager.hpp"

namespace ecs
{
    class UniqueEntity : public RawEntity
    {
    public:
        UniqueEntity() noexcept = default;
        explicit UniqueEntity(RawEntity e)
            : RawEntity {e}
        {}
        ~UniqueEntity()
        {
            getGlobalECSManager()->destroyEntity(*this);
        }

        UniqueEntity(const UniqueEntity&) = delete;
        UniqueEntity(UniqueEntity&& other) noexcept
            : UniqueEntity {other.release()}
        {}
        UniqueEntity& operator= (const UniqueEntity&) = delete;
        UniqueEntity& operator= (UniqueEntity&&) noexcept;

        RawEntity release()
        {
            RawEntity e {.id {this->id}};

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