#pragma once

#include "ecs/raw_entity.hpp"
#include "entity_component_system_manager.hpp"

namespace ecs
{
    class UniqueEntity : public EntityComponentOperationsCRTPBase<UniqueEntity>
    {
    public:
        UniqueEntity() noexcept = default;
        explicit UniqueEntity(RawEntity e)
            : entity {e}
        {}
        ~UniqueEntity()
        {
            this->reset();
        }

        UniqueEntity(const UniqueEntity&) = delete;
        UniqueEntity(UniqueEntity&& other) noexcept
            : UniqueEntity {other.release()}
        {}
        UniqueEntity& operator= (const UniqueEntity&) = delete;
        UniqueEntity& operator= (UniqueEntity&&) noexcept;

        [[nodiscard]] RawEntity getRawEntity() const
        {
            return this->entity;
        }

        [[nodiscard]] RawEntity& getRawEntityRef()
        {
            return this->entity;
        }

        RawEntity release()
        {
            RawEntity e {this->entity};

            this->entity.id = NullEntity;

            return e;
        }

        void reset()
        {
            if (this->entity.id != NullEntity)
            {
                getGlobalECSManager()->destroyEntity(*this);

                this->entity.id = NullEntity;
            }
        }
    private:
        RawEntity entity;
    };

    template<class T, class... Args>
        requires DerivedFromAutoBase<T, InherentEntityBase>
              && std::is_constructible_v<T, UniqueEntity, Args...>
    T* EntityComponentSystemManager::allocateRawInherentEntity(Args&&... args) const
    {
        // TODO: dont use system allocator
        return ::new T {this->createEntity(), std::forward<Args...>(args)...}; // NOLINT
    }

} // namespace ecs