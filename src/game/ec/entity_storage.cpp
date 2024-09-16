#include "entity_storage.hpp"
#include "entity.hpp"
#include <memory>

namespace game::ec
{

    Entity EntityStorage::createEntity()
    {
        const U32 thisEntityId = this->entity_id_allocator.allocate().value();

        const U32 thisGeneration = (*this->metadata)[thisEntityId].generation;

        Entity e;
        e.generation = thisGeneration;
        e.id         = thisEntityId;

        return e;
    }

    void EntityStorage::deleteEntity(Entity e)
    {
        this->entity_id_allocator.free(e.id);

        // make sure this generation number wraps and doesn't collide with e.id
        if ((*this->metadata)[e.id].generation == 16777215)
        {
            (*this->metadata)[e.id].generation = 0;
        }
        else
        {
            (*this->metadata)[e.id].generation += 1;
        }

        util::panic(
            "remove components from pools and set the number of components to "
            "zero");
    }

    bool EntityStorage::isEntityAlive(Entity e)
    {
        return (*this->metadata)[e.id].generation == e.generation;
    }
} // namespace game::ec
