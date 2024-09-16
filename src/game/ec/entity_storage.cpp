#include "entity_storage.hpp"
#include "entity.hpp"
#include <memory>
#include <util/log.hpp>

namespace game::ec
{

    Entity EntityStorage::createEntity()
    {
        const U32 thisEntityId = this->entity_id_allocator.allocate().value();

        const U32 thisGeneration = (*this->metadata)[thisEntityId].generation;

        (*this->metadata)[thisEntityId].number_of_components = 0;
        (*this->metadata)[thisEntityId].component_storage_offset =
            this->storage_0.allocate();

        Entity e;
        e.generation = thisGeneration;
        e.id         = thisEntityId;

        return e;
    }

    void EntityStorage::deleteEntity(Entity e)
    {
        if (!this->isEntityAlive(e))
        {
            util::logWarn("Tried to delete already deleted entity!");
        }
        this->entity_id_allocator.free(e.id);

        EntityMetadata& entityMetadata = (*this->metadata)[e.id];

        // make sure this generation number wraps and doesn't collide with
        // e.id
        if (entityMetadata.generation == 16777215)
        {
            entityMetadata.generation = 0;
        }
        else
        {
            entityMetadata.generation += 1;
        }

        const U8  numberOfComponents = entityMetadata.number_of_components;
        const U32 storageId          = getStoragePoolId(numberOfComponents);

        switch (storageId)
        {
        case 0:
            this->storage_0.free(entityMetadata.component_storage_offset);
            break;
        case 1:
            this->storage_1.free(entityMetadata.component_storage_offset);
            break;
        case 2:
            this->storage_2.free(entityMetadata.component_storage_offset);
            break;
        case 3:
            this->storage_3.free(entityMetadata.component_storage_offset);
            break;
        case 4:
            this->storage_4.free(entityMetadata.component_storage_offset);
            break;
        case 5:
            this->storage_5.free(entityMetadata.component_storage_offset);
            break;
        case 6:
            this->storage_6.free(entityMetadata.component_storage_offset);
            break;
        case 7:
            this->storage_7.free(entityMetadata.component_storage_offset);
            break;
        default:
            util::panic("storage too high!");
        }
    }

    bool EntityStorage::isEntityAlive(Entity e)
    {
        return (*this->metadata)[e.id].generation == e.generation;
    }

    void EntityStorage::addComponentToEntity(Entity e, EntityComponentStorage c)
    {
        if (!this->isEntityAlive(e))
        {
            util::logWarn("tried to add component to dead entity!");
            return;
        }

        EntityMetadata& entityMetadata = (*this->metadata)[e.id];

        if (entityMetadata.number_of_components == MaxComponentsPerEntity)
        {
            util::panic("tried to add too many components");
        }

        const U8 currentNumberOfComponents =
            static_cast<U8>(entityMetadata.number_of_components);
        const U8 newNumberOfComponents =
            static_cast<U8>(currentNumberOfComponents + 1);

        const U8 currentPool = getStoragePoolId(currentNumberOfComponents);
        const U8 newPool     = getStoragePoolId(newNumberOfComponents);

        if (currentPool != newPool)
        {
            // we need to realloc and change pools;
            // change component storage offset and then move from pool to pool

            switch (currentPool)
            {
            case 0: {
                const U32 newOffset = entityMetadata.component_storage_offset =
                    this->storage_1.allocate();

                std::memcpy(
                    this->storage_1.lookup(newOffset).storage.data(),
                    this->storage_0
                        .lookup(entityMetadata.component_storage_offset)
                        .storage.data(),
                    currentNumberOfComponents * sizeof(EntityComponentStorage));

                this->storage_0.free(entityMetadata.component_storage_offset);

                entityMetadata.component_storage_offset = newOffset;

                break;
            }
            case 1: {
                const U32 newOffset = entityMetadata.component_storage_offset =
                    this->storage_2.allocate();

                std::memcpy(
                    this->storage_2.lookup(newOffset).storage.data(),
                    this->storage_1
                        .lookup(entityMetadata.component_storage_offset)
                        .storage.data(),
                    currentNumberOfComponents * sizeof(EntityComponentStorage));

                this->storage_1.free(entityMetadata.component_storage_offset);

                entityMetadata.component_storage_offset = newOffset;

                break;
            }
            case 2: {
                const U32 newOffset = entityMetadata.component_storage_offset =
                    this->storage_3.allocate();

                std::memcpy(
                    this->storage_3.lookup(newOffset).storage.data(),
                    this->storage_2
                        .lookup(entityMetadata.component_storage_offset)
                        .storage.data(),
                    currentNumberOfComponents * sizeof(EntityComponentStorage));

                this->storage_2.free(entityMetadata.component_storage_offset);

                entityMetadata.component_storage_offset = newOffset;

                break;
            }
            case 3: {
                const U32 newOffset = entityMetadata.component_storage_offset =
                    this->storage_4.allocate();

                std::memcpy(
                    this->storage_4.lookup(newOffset).storage.data(),
                    this->storage_3
                        .lookup(entityMetadata.component_storage_offset)
                        .storage.data(),
                    currentNumberOfComponents * sizeof(EntityComponentStorage));

                this->storage_3.free(entityMetadata.component_storage_offset);

                entityMetadata.component_storage_offset = newOffset;

                break;
            }
            case 4: {
                const U32 newOffset = entityMetadata.component_storage_offset =
                    this->storage_5.allocate();

                std::memcpy(
                    this->storage_5.lookup(newOffset).storage.data(),
                    this->storage_4
                        .lookup(entityMetadata.component_storage_offset)
                        .storage.data(),
                    currentNumberOfComponents * sizeof(EntityComponentStorage));

                this->storage_4.free(entityMetadata.component_storage_offset);

                entityMetadata.component_storage_offset = newOffset;

                break;
            }
            case 5: {
                const U32 newOffset = entityMetadata.component_storage_offset =
                    this->storage_6.allocate();

                std::memcpy(
                    this->storage_6.lookup(newOffset).storage.data(),
                    this->storage_5
                        .lookup(entityMetadata.component_storage_offset)
                        .storage.data(),
                    currentNumberOfComponents * sizeof(EntityComponentStorage));

                this->storage_5.free(entityMetadata.component_storage_offset);

                entityMetadata.component_storage_offset = newOffset;

                break;
            }
            case 6: {
                const U32 newOffset = entityMetadata.component_storage_offset =
                    this->storage_7.allocate();

                std::memcpy(
                    this->storage_7.lookup(newOffset).storage.data(),
                    this->storage_6
                        .lookup(entityMetadata.component_storage_offset)
                        .storage.data(),
                    currentNumberOfComponents * sizeof(EntityComponentStorage));

                this->storage_6.free(entityMetadata.component_storage_offset);

                entityMetadata.component_storage_offset = newOffset;

                break;
            }
            case 7:
                this->storage_7.free(entityMetadata.component_storage_offset);
                util::panic("dont get here");
                break;
            default:
                util::panic("pool too hig3h!");
            }
        }

        const U8 idxToAddComponentTo = entityMetadata.number_of_components;
        entityMetadata.number_of_components += 1;

        switch (newPool)
        {
        case 0:
            this->storage_0.lookup(entityMetadata.component_storage_offset)
                .storage[idxToAddComponentTo] = c;
            break;
        case 1:
            this->storage_1.lookup(entityMetadata.component_storage_offset)
                .storage[idxToAddComponentTo] = c;
            break;
        case 2:
            this->storage_2.lookup(entityMetadata.component_storage_offset)
                .storage[idxToAddComponentTo] = c;
            break;
        case 3:
            this->storage_3.lookup(entityMetadata.component_storage_offset)
                .storage[idxToAddComponentTo] = c;
            break;
        case 4:
            this->storage_4.lookup(entityMetadata.component_storage_offset)
                .storage[idxToAddComponentTo] = c;
            break;
        case 5:
            this->storage_5.lookup(entityMetadata.component_storage_offset)
                .storage[idxToAddComponentTo] = c;
            break;
        case 6:
            this->storage_6.lookup(entityMetadata.component_storage_offset)
                .storage[idxToAddComponentTo] = c;
            break;
        case 7:
            this->storage_7.lookup(entityMetadata.component_storage_offset)
                .storage[idxToAddComponentTo] = c;
            break;
        default:
            util::panic("pool too high!");
        }
    }

} // namespace game::ec
