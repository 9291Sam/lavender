#pragma once

#include "entity.hpp"
#include <bit>
#include <type_traits>

namespace game::ec
{
    class EntityStorage
    {
    public:
        struct EntityMetadata
        {
            U32 generation;
            U32 number_of_components;
        };

        struct EntityComponentStorage // TODO: this could be packed into a U8
                                      // and a U24
        {
            // This is the constexpr determined at compile time id
            U32 component_type_id;
            // this is the component;s offset in whatever else stores it
            U32 component_storage_offset;
        };
        static_assert(std::has_unique_object_representations_v<EntityMetadata>);
        static_assert(
            std::has_unique_object_representations_v<EntityComponentStorage>);
        static constexpr U8 MaxComponentsPerEntity = 255;
        static constexpr std::size_t
        getStoragePoolId(std::size_t numberOfComponents)
        {
            return std::bit_width(numberOfComponents);
        }
    public:

        EntityStorage();
        ~EntityStorage() noexcept;

        EntityStorage(const EntityStorage&)             = delete;
        EntityStorage(EntityStorage&&)                  = delete;
        EntityStorage& operator= (const EntityStorage&) = delete;
        EntityStorage& operator= (EntityStorage&&)      = delete;

        [[nodiscard]] Entity createEntity(std::size_t componentEstimate);
        void                 deleteEntity(Entity);

        void addComponentToEntity(Entity, EntityComponentStorage);
        void removeComponentFromEntity(Entity, EntityComponentStorage);

        bool entityHasComponent(Entity, EntityComponentStorage);

    private:

        static constexpr std::size_t MaxEntities = 1048576;

        std::unique_ptr<std::array<EntityMetadata, MaxEntities>> metadata;
        std::unique_ptr < std::array <

        // Entity -> EntityMetadata
        // EntityMetadata -> Component Lists
    };
} // namespace game::ec