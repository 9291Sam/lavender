#pragma once

#include "raw_entity.hpp"

namespace ecs
{
    class EntityComponentSystemManager
    {
    public:
        template<class C>
        void addComponent(RawEntity e, ...) const;

        void destroyEntity(RawEntity e) const;
    };
} // namespace ecs