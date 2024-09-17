#include "entity_storage.hpp"
#include "entity.hpp"
#include <__expected/unexpected.h>
#include <memory>
#include <optional>
#include <type_traits>
#include <util/log.hpp>
#include <utility>

namespace game::ec
{

    EntityStorage::EntityStorage()
        : entity_id_allocator {MaxEntities}
        , metadata {std::make_unique<std::array<EntityMetadata, MaxEntities>>()}
        , storage_0 {128}
        , storage_1 {128}
        , storage_2 {128}
        , storage_3 {128}
        , storage_4 {128}
        , storage_5 {128}
        , storage_6 {128}
        , storage_7 {128}
    {}

    EntityStorage::~EntityStorage() noexcept = default;

    Entity EntityStorage::create()
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

    std::expected<void, EntityStorage::EntityDead>
    EntityStorage::destroy(Entity e)
    {
        if (!this->isAlive(e))
        {
            return std::unexpected(EntityDead {});
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

        util::panic("unreachable!");
        return {};
    }

    bool EntityStorage::isAlive(Entity e)
    {
        return (*this->metadata)[e.id].generation == e.generation;
    }

    std::expected<void, EntityStorage::ComponentModificationError>
    EntityStorage::addComponent(Entity e, EntityComponentStorage c)
    {
        if (!this->isAlive(e))
        {
            return std::unexpected(ComponentModificationError::EntityDead);
        }

        EntityMetadata& entityMetadata = (*this->metadata)[e.id];

        if (entityMetadata.number_of_components == MaxComponentsPerEntity)
        {
            util::panic("tried to add too many components");
        }

        if (this->hasComponent(e, c))
        {
            return std::unexpected(
                ComponentModificationError::ComponentConflict);
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

            auto moveComponentsToNewPool =
                [&]<std::size_t N>(
                    StoredEntityComponentsList<N>&                 oldPool,
                    StoredEntityComponentsList<((N + 1) * 2) - 1>& newPool)
            {
                const U32 newOffset = entityMetadata.component_storage_offset =
                    newPool.allocate();

                std::memcpy(
                    newPool.lookup(newOffset).storage.data(),
                    oldPool.lookup(entityMetadata.component_storage_offset)
                        .storage.data(),
                    currentNumberOfComponents * sizeof(EntityComponentStorage));

                oldPool.free(entityMetadata.component_storage_offset);

                entityMetadata.component_storage_offset = newOffset;
            };

            switch (currentPool)
            {
            case 0:
                moveComponentsToNewPool(this->storage_0, this->storage_1);
                break;
            case 1:
                moveComponentsToNewPool(this->storage_1, this->storage_2);
                break;
            case 2:
                moveComponentsToNewPool(this->storage_2, this->storage_3);
                break;
            case 3:
                moveComponentsToNewPool(this->storage_3, this->storage_4);
                break;
            case 4:
                moveComponentsToNewPool(this->storage_4, this->storage_5);
                break;
            case 5:
                moveComponentsToNewPool(this->storage_5, this->storage_6);
                break;
            case 6:
                moveComponentsToNewPool(this->storage_6, this->storage_7);
                break;
            default:
                util::panic(
                    "EntityStorage::addComponentToEntity new pool {} too high",
                    currentPool);
            }
        }

        const U8 idxToAddComponentTo = entityMetadata.number_of_components;
        entityMetadata.number_of_components += 1;

        // insert the new component
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

    std::expected<void, EntityStorage::ComponentModificationError>
    EntityStorage::removeComponent(Entity e, EntityComponentStorage c)
    {
        if (!this->isAlive(e))
        {
            return std::unexpected(ComponentModificationError::EntityDead);
        }

        const std::optional<U8> idx = this->getIndexOfComponentUnchecked(e, c);

        if (idx.has_value())
        {
            return std::unexpected(
                ComponentModificationError::ComponentConflict);
        }

        EntityMetadata& entityMetadata         = (*this->metadata)[e.id];
        const U8        idxOfComponentToRemove = *idx;
        const U8 idxOfComponentToMove = entityMetadata.number_of_components - 1;

        const U8 originalPool =
            getStoragePoolId(entityMetadata.number_of_components);

        // move top down and decrement size

        auto moveDeleteOneComponent =
            [&]<std::size_t N>(StoredEntityComponentsList<N>& storage)
        {
            StoredEntityComponents<N>& components =
                storage.lookup(entityMetadata.component_storage_offset);

            components.storage[idxOfComponentToRemove] =
                components.storage[idxOfComponentToMove];
        };

        switch (originalPool)
        {
        case 0:
            moveDeleteOneComponent(this->storage_0);
            break;
        case 1:
            moveDeleteOneComponent(this->storage_1);
            break;
        case 2:
            moveDeleteOneComponent(this->storage_2);
            break;
        case 3:
            moveDeleteOneComponent(this->storage_3);
            break;
        case 4:
            moveDeleteOneComponent(this->storage_4);
            break;
        case 5:
            moveDeleteOneComponent(this->storage_5);
            break;
        case 6:
            moveDeleteOneComponent(this->storage_6);
            break;
        case 7:
            moveDeleteOneComponent(this->storage_7);
            break;
        default:
            util::panic("pool too high!");
        }

        entityMetadata.number_of_components -= 1;

        const U8 poolAfterRemoval =
            getStoragePoolId(entityMetadata.number_of_components);

        if (originalPool != poolAfterRemoval)
        {
            auto moveComponentsDown =
                [&]<std::size_t N>(
                    StoredEntityComponentsList<((N + 1) * 2) - 1>& oldPool,
                    StoredEntityComponentsList<N>&                 newPool)
            {
                const U32 newOffset = entityMetadata.component_storage_offset =
                    newPool.allocate();

                std::memcpy(
                    newPool.lookup(newOffset).storage.data(),
                    oldPool.lookup(entityMetadata.component_storage_offset)
                        .storage.data(),
                    entityMetadata.number_of_components
                        * sizeof(EntityComponentStorage));

                oldPool.free(entityMetadata.component_storage_offset);

                entityMetadata.component_storage_offset = newOffset;
            };

            switch (poolAfterRemoval)
            {
            case 0:
                moveComponentsDown(this->storage_1, this->storage_0);
                break;
            case 1:
                moveComponentsDown(this->storage_2, this->storage_1);
                break;
            case 2:
                moveComponentsDown(this->storage_3, this->storage_2);
                break;
            case 3:
                moveComponentsDown(this->storage_4, this->storage_3);
                break;
            case 4:
                moveComponentsDown(this->storage_5, this->storage_4);
                break;
            case 5:
                moveComponentsDown(this->storage_6, this->storage_5);
                break;
            case 6:
                moveComponentsDown(this->storage_7, this->storage_6);
                break;
            default:
                util::panic(
                    "EntityStorage::moveComponentsDown new pool {} too high",
                    poolAfterRemoval);
            }
        }
    }

    std::expected<bool, EntityStorage::EntityDead>
    EntityStorage::hasComponent(Entity e, EntityComponentStorage c)
    {
        if (!this->isAlive(e))
        {
            return std::unexpected(EntityDead {});
        }

        return this->getIndexOfComponentUnchecked(e, c).has_value();
    }

    std::optional<U8> EntityStorage::getIndexOfComponentUnchecked(
        Entity e, EntityComponentStorage c)
    {
        const EntityMetadata& entityMetadata = (*this->metadata)[e.id];

        const U8 pool = getStoragePoolId(entityMetadata.number_of_components);

        auto doLookup =
            [&]<std::size_t N>(
                StoredEntityComponentsList<N>& storage) -> std::optional<U8>
        {
            const std::array<EntityComponentStorage, N>& components =
                storage.lookup(entityMetadata.component_storage_offset).storage;

            for (std::size_t i = 0; i < entityMetadata.number_of_components;
                 ++i)
            {
                if (components[i] == c)
                {
                    return i;
                }
            }

            return std::nullopt;
        };

        switch (pool)
        {
        case 0:
            return doLookup(this->storage_0);
        case 1:
            return doLookup(this->storage_1);
        case 2:
            return doLookup(this->storage_2);
        case 3:
            return doLookup(this->storage_3);
        case 4:
            return doLookup(this->storage_4);
        case 5:
            return doLookup(this->storage_5);
        case 6:
            return doLookup(this->storage_6);
        case 7:
            return doLookup(this->storage_7);
        default:
            util::panic("pool too high");
        }

        return std::nullopt;
    }
} // namespace game::ec
