#pragma once

#include "game/ec/components.hpp"
#include "game/transform.hpp"

namespace verdigris
{
    struct TriangleComponent : game::ComponentBase<TriangleComponent>
    {
        game::Transform transform;
    };
} // namespace verdigris
