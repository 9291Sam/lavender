#pragma once

#include "entity.hpp"
#include "game/ec/components.hpp"
#include <array>
#include <bit>
#include <expected>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <util/index_allocator.hpp>

namespace game::ec
{

    enum class ComponentModificationError
    {
        EntityDead,
        ComponentConflict
    };

    struct EntityDead
    {};

    class EntityStorage
    {
    public:
        struct EntityMetadata
        {
            u32 number_of_components : 8;
            u32 generation           : 24;
            u32 component_storage_offset;

            operator std::string () const
            {
                return std::format(
                    "EntityMetadata | {} Components | Generation {} | Offset: "
                    "{}",
                    this->number_of_components,
                    this->generation,
                    this->component_storage_offset);
            }
        };
        static_assert(sizeof(EntityMetadata) == sizeof(u64));

        struct EntityComponentStorage
        {
            u32 component_type_id        : 8;
            u32 component_storage_offset : 24;

            constexpr bool
            operator== (const EntityComponentStorage&) const noexcept = default;
        };
        static_assert(
            std::has_unique_object_representations_v<EntityComponentStorage>);
        static constexpr u8 MaxComponentsPerEntity = 255;
        static constexpr u8 getStoragePoolId(u8 numberOfComponents)
        {
            return static_cast<u8>(
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

            explicit StoredEntityComponentsList(u32 numberOfEntitiesToStore)
                : internal_allocator {numberOfEntitiesToStore}
                , storage {}
            {
                this->storage.resize(numberOfEntitiesToStore);
            }

            StoredEntityComponents<N>& lookup(u32 id)
            {
                return this->storage[id];
            }

            u32 allocate()
            {
                if (std::expected<u32, util::IndexAllocator::OutOfBlocks> id =
                        this->internal_allocator.allocate();
                    id.has_value())
                {
                    return *id;
                }
                else
                {
                    this->internal_allocator.updateAvailableBlockAmount(
                        static_cast<u32>(this->storage.size()) * 2);
                    this->storage.resize(this->storage.size() * 2);

                    return this->internal_allocator.allocate()
                        .value(); // NOLINT
                }
            }
            void free(u32 id)
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
        // Returns true if the entity was succesfully destroyed
        [[nodiscard]] bool   destroy(Entity);

        [[nodiscard]] bool isAlive(Entity);

        [[nodiscard]] std::expected<void, ComponentModificationError>
            addComponent(Entity, EntityComponentStorage);
        [[nodiscard]] std::
            expected<EntityComponentStorage, ComponentModificationError>
                removeComponent(Entity, ComponentTypeId);

        [[nodiscard]] std::expected<bool, EntityDead>
            hasComponent(Entity, ComponentTypeId);

        [[nodiscard]] std::
            expected<EntityComponentStorage, ComponentModificationError>
                readComponent(Entity, ComponentTypeId);

        [[nodiscard]] std::span<const EntityComponentStorage>
            getAllComponents(Entity);

    private:

        std::optional<u8> getIndexOfComponentUnchecked(Entity, ComponentTypeId);

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
