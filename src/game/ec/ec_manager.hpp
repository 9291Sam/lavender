#pragma once

#include "components.hpp"
#include "entity.hpp"
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
    static_assert(FooComponent::getSize() == 1);

    class ECManager
    {
    public:
        template<Component C>
        struct StoredComponent
        {
            C      component;
            Entity parent;

            bool isAlive() const
            {
                return !this->parent.isNull();
            }
        };

        template<Component C>
        struct ComponentArray
        {
            std::size_t        number_of_valid_components;
            std::size_t        number_of_allocated_components;
            // util::IndexAllocator index_allocator;
            StoredComponent<C> storage[]; // NOLINT
        };

        struct MuckedComponent
        {
            Entity                                        entity;
            ComponentId                                   id;
            boost::container::small_vector<std::byte, 64> component;

            bool operator== (const MuckedComponent&) const = default;
        };

    public:

        explicit ECManager();
        ~ECManager() noexcept;

        ECManager(const ECManager&)             = delete;
        ECManager(ECManager&&)                  = delete;
        ECManager& operator= (const ECManager&) = delete;
        ECManager& operator= (ECManager&&)      = delete;

        // Entity createEntity(std::size_t componentEstimate = 3) const;

        // template<Component C>
        // void addComponent(Entity entity, C component) const
        //     requires std::is_trivially_copyable_v<C>
        //           && std::is_standard_layout_v<C>
        // {
        //     const std::shared_lock lock {this->access_lock};

        //     const std::span<const std::byte> componentBytes =
        //         component.asBytes();

        //     this->components_to_add.insert(MuckedComponent {
        //         .entity {entity},
        //         .id {C::Id},
        //         .component {boost::container::small_vector<std::byte, 64> {
        //             componentBytes.begin(), componentBytes.end()}}});
        // }

        // template<Component C>
        // void iterateComponents(std::invocable<Entity, const C&> auto func)
        // const
        // {
        //     const std::size_t componentId = static_cast<std::size_t>(C::Id);

        //     const ComponentArray<C>* componentStorage =
        //         reinterpret_cast<const ComponentArray<C>*>(
        //             this->component_storage[componentId].get());

        //     for (std::size_t i = 0;
        //          i < componentStorage->number_of_valid_components;
        //          ++i)
        //     {
        //         if (componentStorage->storage[i].isAlive())
        //         {
        //             func(
        //                 componentStorage->storage[i].parent,
        //                 componentStorage->storage[i].component);
        //         }
        //     }
        // }

        void flush();

    private:
        mutable std::shared_mutex access_lock;
        std::array<std::unique_ptr<U8[]>, NumberOfGameComponents>
            component_storage;
        mutable boost::unordered::concurrent_flat_set<MuckedComponent>
            component_deltas;
    };

    // NOLINTNEXTLINE
    inline std::size_t hash_value(const game::ec::ECManager::MuckedComponent& m)
    {
        std::size_t seed = 8234897234798432;

        util::hashCombine(seed, hash_value(m.entity));
        util::hashCombine(seed, boost::hash_value(m.id));
        util::hashCombine(seed, boost::hash_value(m.component));

        return seed;
    }
} // namespace game::ec
