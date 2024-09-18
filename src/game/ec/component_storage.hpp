#pragma once

#include "components.hpp"
#include "entity.hpp"
#include "util/type_erasure.hpp"
#include <util/index_allocator.hpp>
#include <util/log.hpp>
#include <util/misc.hpp>

namespace game::ec
{
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
            // if (this->component_destructor == nullptr)
            // {
            //     this->component_destructor = util::TypeErasedDestructor {
            //         util::ZSTTypeWrapper<C> {}}.destructor;
            // }

            util::assertFatal(sizeof(C) == this->component_size, "oops");

            std::expected<U32, util::IndexAllocator::OutOfBlocks> id =
                this->allocator.allocate();

            if (!id.has_value())
            {
                const std::size_t newSize = this->parent_entities.size() * 2;

                this->allocator.updateAvailableBlockAmount(newSize);
                this->parent_entities.resize(newSize);
                this->component_storage.resize(newSize * this->component_size);

                id = this->allocator.allocate();
            }

            const U32 componentId = id.value();

            this->parent_entities[componentId] = parentEntity;
            reinterpret_cast<C*>(this->component_storage.data())[componentId] =
                c;

            return componentId;
        }

        template<class C>
        [[nodiscard]] C deleteComponent(U32 index)
        {
            this->parent_entities[index] = Entity {};
            this->allocator.free(index);
            return std::move(
                reinterpret_cast<C*>(this->component_storage.data())[index]);
        }

        void deleteComponent(U32 index, void (*destructor)(void*) noexcept)
        {
            this->parent_entities[index] = Entity {};
            this->allocator.free(index);

#warning wrong
            // destructor(
            //     this->component_storage.data()
            //     + (index * this->component_size));
        }

        template<Component C>
        void iterateComponents(std::invocable<Entity, C&> auto func)
        {
            util::assertFatal(sizeof(C) == this->component_size, "oops");

            C* storedComponents =
                reinterpret_cast<C*>(this->component_storage.data());

            for (int i = 0; i < this->allocator.getAllocatedBlocksUpperBound();
                 ++i)
            {
                if (!this->parent_entities[i].isNull())
                {
                    func(this->parent_entities[i], storedComponents[i]);
                }
            }
        }

        // unspellable type :)
        auto getDestructor() noexcept
        {
            return this->component_destructor;
        }

        MuckedComponentStorage(const MuckedComponentStorage&) = delete;
        MuckedComponentStorage(MuckedComponentStorage&&)      = default;
        MuckedComponentStorage&
        operator= (const MuckedComponentStorage&)                    = delete;
        MuckedComponentStorage& operator= (MuckedComponentStorage&&) = default;

    private:
        void (*component_destructor)(void*) noexcept;
        util::IndexAllocator allocator;
        U32                  component_size;
        std::vector<Entity>  parent_entities;
        std::vector<U8>      component_storage;
    };

} // namespace game::ec
