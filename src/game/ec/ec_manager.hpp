#pragma once

#include "components.hpp"
#include "entity.hpp"
#include "game/ec/entity_storage.hpp"
#include "game/render/triangle_component.hpp"
#include "util/index_allocator.hpp"
#include "util/log.hpp"
#include "util/threads.hpp"
#include <array>
#include <boost/container/small_vector.hpp>
#include <boost/unordered/concurrent_flat_set.hpp>
#include <concepts>
#include <cstddef>
#include <memory>
#include <shared_mutex>
#include <string>
#include <type_traits>
#include <util/misc.hpp>
#include <variant>
#include <vector>

namespace game::ec
{
    struct BarComponent : ComponentBase<BarComponent>
    {};

    struct FooComponent : ComponentBase<FooComponent>
    {
        U8 a;
    };

    static_assert(FooComponent::Id == 0);
    static_assert(BarComponent::Id == 1);

    class ECManager
    {
    public:

        explicit ECManager()
            : component_storage {
                  util::Mutex {
                      MuckedComponentStorage(128, getComponentSize<0>())},
                  util::Mutex {
                      MuckedComponentStorage(128, getComponentSize<1>())},
                  util::Mutex {
                      MuckedComponentStorage(128, getComponentSize<2>())}}
        {}
        ~ECManager() noexcept = default;

        ECManager(const ECManager&)             = delete;
        ECManager(ECManager&&)                  = delete;
        ECManager& operator= (const ECManager&) = delete;
        ECManager& operator= (ECManager&&)      = delete;

        Entity createEntity() const
        {
            return this->entity_storage.lock(
                [](EntityStorage& storage)
                {
                    return storage.createEntity();
                });
        }

        template<Component C>
        void addComponent(Entity entity, C component) const
            requires (
                std::is_trivially_copyable_v<C> && std::is_standard_layout_v<C>
                && std::is_trivially_destructible_v<C>
                && getComponentSize<getComponentId<C>()>() == sizeof(C))
        {
            const std::span<const std::byte> componentBytes =
                component.asBytes();

            const U32 componentStorageOffset =
                this->component_storage[C::Id].lock(
                    [&](MuckedComponentStorage& storage)
                    {
                        return storage.insertComponent(entity, component);
                    });

            this->entity_storage.lock(
                [&](EntityStorage& storage)
                {
                    storage.addComponentToEntity(
                        entity,
                        EntityStorage::EntityComponentStorage {
                            .component_storage_offset {componentStorageOffset},
                            .component_type_id {C::Id}});
                });
        }

        template<Component C>
        void iterateComponents(std::invocable<Entity, const C&> auto func) const
        {
            const std::size_t componentId = static_cast<std::size_t>(C::Id);

            this->component_storage[C::Id].lock(
                [&](MuckedComponentStorage& storage)
                {
                    storage.iterateComponents<C>(func);
                });
        }

    private:

        class MuckedComponentStorage
        {
        public:
            MuckedComponentStorage(
                U32 componentsToAllocate, std::size_t componentSize) // NOLINT
                : allocator {componentsToAllocate}
                , component_size {static_cast<U32>(componentSize)}
            {
                this->parent_entities.resize(componentsToAllocate, Entity {});
                this->component_storage.resize(static_cast<std::size_t>(
                    componentsToAllocate * this->component_size));
            }

            template<Component C>
            U32 insertComponent(Entity parentEntity, C c)
            {
                util::assertFatal(sizeof(C) == this->component_size, "oops");

                std::expected<U32, util::IndexAllocator::OutOfBlocks> id =
                    this->allocator.allocate();

                if (!id.has_value())
                {
                    const std::size_t newSize =
                        this->parent_entities.size() * 2;

                    this->allocator.updateAvailableBlockAmount(newSize);
                    this->parent_entities.resize(newSize);
                    this->component_storage.resize(
                        newSize * this->component_size);

                    id = this->allocator.allocate();
                }

                const U32 componentId = id.value();

                this->parent_entities[componentId] = parentEntity;
                reinterpret_cast<C*>(
                    this->component_storage.data())[componentId] = c;

                return componentId;
            }

            void deleteComponent(U32 index)
            {
                this->parent_entities[index] = Entity {};
                this->allocator.free(index);
            }

            template<Component C>
            void iterateComponents(std::invocable<Entity, C&> auto func)
            {
                util::assertFatal(sizeof(C) == this->component_size, "oops");
                C* storedComponents =
                    reinterpret_cast<C*>(this->component_storage.data());

                for (int i = 0;
                     i < this->allocator.getAllocatedBlocksUpperBound();
                     ++i)
                {
                    if (!this->parent_entities[i].isNull())
                    {
                        func(this->parent_entities[i], storedComponents[i]);
                    }
                }
            }

            MuckedComponentStorage(const MuckedComponentStorage&) = delete;
            MuckedComponentStorage(MuckedComponentStorage&&)      = default;
            MuckedComponentStorage&
            operator= (const MuckedComponentStorage&) = delete;
            MuckedComponentStorage&
            operator= (MuckedComponentStorage&&) = default;

        private:
            util::IndexAllocator allocator;
            U32                  component_size;
            std::vector<Entity>  parent_entities;
            std::vector<U8>      component_storage;
        };

        std::array<util::Mutex<MuckedComponentStorage>, NumberOfGameComponents>
                                   component_storage;
        util::Mutex<EntityStorage> entity_storage;
    };

} // namespace game::ec
