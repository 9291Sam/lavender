#pragma once

#include "game/transform.hpp"
#include <game/ec/components.hpp>
#include <glm/common.hpp>
#include <util/log.hpp>

namespace game::render
{
    struct TriangleComponent : ComponentBase<TriangleComponent>
    {
        Transform transform;
    };
} // namespace game::render
