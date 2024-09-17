#pragma once

#include "component_storage.hpp"
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

        explicit ECManager();
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
            this->component_storage[C::Id].lock(
                [&](MuckedComponentStorage& storage)
                {
                    storage.iterateComponents<C>(func);
                });
        }

    private:

        std::array<util::Mutex<MuckedComponentStorage>, NumberOfGameComponents>
                                   component_storage;
        util::Mutex<EntityStorage> entity_storage;
    };

} // namespace game::ec
