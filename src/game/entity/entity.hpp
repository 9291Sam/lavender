#pragma once

namespace entity
{
    class Entity
    {};

    class ConcreteEntity : Entity
    {
    public:
        virtual ~ConcreteEntity() = 0;
    };
} // namespace entity