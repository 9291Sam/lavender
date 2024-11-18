#pragma once

#include "ecs/raw_entity.hpp"
#include "entity_component_system_manager.hpp"
#include "util/log.hpp"
#include <ctti/type_id.hpp>
#include <source_location>

namespace ecs
{

    template<class C>
    auto EntityComponentSystemManager::tryAddComponent(RawEntity e, C&& c) const -> Result
    {
        bool added = false;

        const std::size_t visted = this->entities_guard->cvisit(
            e.id,
            [&](const auto&)
            {
                this->accessComponentStorage<C>(
                    [&](const boost::unordered::concurrent_flat_map<u32, C>& storage)
                    {
                        added = const_cast<boost::unordered::concurrent_flat_map<u32, C>&>(storage)
                                    .insert({e.id, std::forward<C>(c)});
                    });
            });

        if (visted == 0)
        {
            return Result::EntityDead;
        }
        else if (added)
        {
            return Result::NoError;
        }
        else
        {
            return Result::ComponentConflict;
        }
    }

    template<class C>
    void EntityComponentSystemManager::addComponent(
        RawEntity e, C&& c, std::source_location location) const
    {
        const Result result = this->tryAddComponent(e, std::forward<C>(c));

        switch (result)
        {
        case Result::NoError:
            break;
        case Result::ComponentConflict:
            util::logWarn<>("Duplicate insertion of component", location);
            break;
        case Result::EntityDead:
            util::logWarn<>("Tried to insert component on dead entity", location);
            break;
        }
    }

} // namespace ecs
