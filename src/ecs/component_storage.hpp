#pragma once

#include "util/misc.hpp"
#include <boost/unordered/concurrent_flat_map.hpp>

namespace ecs
{
    struct ComponentStorageBase
    {
        ComponentStorageBase()          = default;
        virtual ~ComponentStorageBase() = default;

        virtual void* getRawStorage()                    = 0;
        virtual void  clearAllComponentsForId(u32) const = 0;
    };

    template<class C>
    struct ComponentStorage : ComponentStorageBase
    {
        ComponentStorage()           = default;
        ~ComponentStorage() override = default;

        void* getRawStorage() override
        {
            return &this->storage;
        }

        void clearAllComponentsForId(u32 id) const override
        {
            this->storage.erase(id);
        }

        mutable boost::unordered::concurrent_flat_map<u32, C> storage;
    };
} // namespace ecs