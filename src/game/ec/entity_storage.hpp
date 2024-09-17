#pragma once

#include "entity.hpp"
#include <array>
#include <bit>
#include <memory>
#include <optional>
#include <type_traits>
#include <util/index_allocator.hpp>

namespace game::ec
{
    class EntityStorage
    {
    public:
        struct EntityMetadata
        {
            U32 number_of_components : 8;
            U32 generation           : 24;
            U32 component_storage_offset;
        };
        static_assert(sizeof(EntityMetadata) == sizeof(U64));

        struct EntityComponentStorage
        {
            U32 component_type_id        : 8;
            U32 component_storage_offset : 24;

            constexpr bool
            operator== (const EntityComponentStorage&) const noexcept = default;
        };
        static_assert(
            std::has_unique_object_representations_v<EntityComponentStorage>);
        static constexpr U8 MaxComponentsPerEntity = 255;
        static constexpr U8 getStoragePoolId(U8 numberOfComponents)
        {
            return static_cast<U8>(
                std::max({1, std::bit_width(numberOfComponents)}) - 1);
        }

        template<std::size_t N>
        struct StoredEntityComponents
        {
            std::array<EntityComponentStorage, N> storage;
        };

        template<std::size_t N>
        class StoredEntityComponentsList
        {
        public:

            explicit StoredEntityComponentsList(U32 numberOfEntitiesToStore)
                : internal_allocator {numberOfEntitiesToStore}
                , storage {}
            {
                this->storage.resize(numberOfEntitiesToStore);
            }

            StoredEntityComponents<N>& lookup(U32 id)
            {
                return this->storage[id];
            }

            U32 allocate()
            {
                if (std::expected<U32, util::IndexAllocator::OutOfBlocks> id =
                        this->internal_allocator.allocate();
                    id.has_value())
                {
                    return *id;
                }
                else
                {
                    this->internal_allocator.updateAvailableBlockAmount(
                        this->storage.size() * 2);
                    this->storage.resize(this->storage.size() * 2);

                    return this->internal_allocator.allocate()
                        .value(); // NOLINT
                }
            }
            void free(U32 id)
            {
                this->internal_allocator.free(id);
            }

        private:

            util::IndexAllocator                   internal_allocator;
            std::vector<StoredEntityComponents<N>> storage;
        };

    public:

        EntityStorage();
        ~EntityStorage() noexcept;

        EntityStorage(const EntityStorage&)             = delete;
        EntityStorage(EntityStorage&&)                  = delete;
        EntityStorage& operator= (const EntityStorage&) = delete;
        EntityStorage& operator= (EntityStorage&&)      = delete;

        [[nodiscard]] Entity create();
        [[nodiscard]] bool   destroy(Entity);

        [[nodiscard]] bool isAlive(Entity);

        [[nodiscard]] bool addComponent(Entity, EntityComponentStorage);
        [[nodiscard]] bool removeComponent(Entity, EntityComponentStorage);
        [[nodiscard]] bool hasComponent(Entity, EntityComponentStorage);

    private:

        std::optional<U8> getIndexOfComponent(Entity, EntityComponentStorage);

        static constexpr std::size_t MaxEntities = 1048576;

        // TODO: make this auto resize instead
        util::IndexAllocator entity_id_allocator;
        std::unique_ptr<std::array<EntityMetadata, MaxEntities>> metadata;

        StoredEntityComponentsList<1>   storage_0;
        StoredEntityComponentsList<3>   storage_1;
        StoredEntityComponentsList<7>   storage_2;
        StoredEntityComponentsList<15>  storage_3;
        StoredEntityComponentsList<31>  storage_4;
        StoredEntityComponentsList<63>  storage_5;
        StoredEntityComponentsList<127> storage_6;
        StoredEntityComponentsList<255> storage_7;
    };
} // namespace game::ec
