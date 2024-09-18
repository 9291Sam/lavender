#pragma once

#include "components.hpp"
#include "entity.hpp"
#include "util/index_allocator.hpp"
#include "util/misc.hpp"
#include "util/type_erasure.hpp"
#include <memory>
#include <new>
#include <optional>
#include <type_traits>

namespace game::ec
{
    class TypeErasedComponentStorage
    {
    public:
        TypeErasedComponentStorage() noexcept  = default;
        ~TypeErasedComponentStorage() noexcept = default;

        TypeErasedComponentStorage(const TypeErasedComponentStorage&) = delete;
        TypeErasedComponentStorage(TypeErasedComponentStorage&&)      = delete;
        TypeErasedComponentStorage&
        operator= (const TypeErasedComponentStorage&) = delete;
        TypeErasedComponentStorage&
        operator= (TypeErasedComponentStorage&&) = delete;

        template<Component C>
        [[nodiscard]] U32 addComponent(Entity parent, C&& c)
        {
            if (this->initalized_data == nullptr)
            {
                this->initalized_data = std::make_unique<InitalizedData>(
                    util::ZSTTypeWrapper<C> {});
            }

            return this->initalized_data->addComponent<C>(
                parent, std::forward<C>(c));
        }

        template<Component C>
        [[nodiscard]] C takeComponent(U32 index)
        {
            return this->initalized_data->takeComponent<C>(index);
        }

        void deleteComponent(U32 index)
        {
            this->initalized_data->deleteComponent(index);
        }

        template<Component C>
        void iterateComponents(std::invocable<Entity, C&> auto func)
        {
            this->initalized_data->iterateComponents<C>(func);
        }

        template<Component C>
        C& modifyComponent(U32 index)
        {
            return this->initalized_data->modifyComponent<C>(index);
        }

    private:

        struct InitalizedData
        {
            static constexpr std::size_t DefaultComponentsAmount = 1024;

            template<class T>
            InitalizedData(util::ZSTTypeWrapper<T> t)
                : type_info {t}
                , allocator {DefaultComponentsAmount}
                , parent_entities {DefaultComponentsAmount, Entity {}}
                , owned_storage {reinterpret_cast<std::byte*>(operator new (
                      sizeof(T) * DefaultComponentsAmount,
                      std::align_val_t {alignof(T)}))}
            {}

            ~InitalizedData()
            {
                this->allocator.iterateThroughAllocatedElements(
                    [&](std::size_t idxToDestroy)
                    {
                        this->type_info.destructor(
                            this->owned_storage
                            + this->type_info.size * idxToDestroy);
                    });

                operator delete (
                    this->owned_storage,
                    std::align_val_t {this->type_info.align});
            }

            void realloc(std::size_t newLength)
            {
                this->allocator.updateAvailableBlockAmount(newLength);
                this->parent_entities.resize(newLength, Entity {});

                std::byte* newOwnedStorage =
                    reinterpret_cast<std::byte*>(operator new (
                        this->type_info.size * newLength,
                        std::align_val_t {this->type_info.align}));

                this->allocator.iterateThroughAllocatedElements(
                    [&](std::size_t idxToMoveToNewArray)
                    {
                        this->type_info.uninitialized_move_constructor(
                            newOwnedStorage
                                + this->type_info.size * idxToMoveToNewArray,
                            this->owned_storage
                                + this->type_info.size * idxToMoveToNewArray);

                        this->type_info.destructor(
                            this->owned_storage
                            + this->type_info.size * idxToMoveToNewArray);
                    });
                operator delete (
                    this->owned_storage,
                    std::align_val_t {this->type_info.align});
                this->owned_storage = newOwnedStorage;
            }

            template<Component C>
                requires std::is_default_constructible_v<C>
                      && std::is_nothrow_move_constructible_v<C>
            U32 addComponent(Entity parent, C&& c)
            {
                std::expected<U32, util::IndexAllocator::OutOfBlocks>
                    allocationResult = this->allocator.allocate();

                if (!allocationResult.has_value())
                {
                    this->realloc(this->parent_entities.size() * 2);

                    allocationResult = this->allocator.allocate();
                }

                U32 indexToInsert = *allocationResult;

                this->parent_entities[indexToInsert] = parent;

                new (this->owned_storage + sizeof(C) * indexToInsert)
                    C {std::forward<C>(c)};

                return indexToInsert;
            }

            template<class C>
            C& modifyComponent(U32 idx)
            {
                return *reinterpret_cast<C*>(
                    this->owned_storage + sizeof(C) * idx);
            }

            template<class C>
            C takeComponent(U32 idx)
            {
                this->allocator.free(idx);
                this->parent_entities[idx] = Entity {};

                C out {std::move(*reinterpret_cast<C*>(
                    this->owned_storage + this->type_info.size * idx))};

                this->type_info.destructor(
                    this->owned_storage + this->type_info.size * idx);

                return out;
            }

            void deleteComponent(U32 idx)
            {
                this->allocator.free(idx);
                this->parent_entities[idx] = Entity {};
                this->type_info.destructor(
                    this->owned_storage + this->type_info.size * idx);
            }

            template<Component C>
            void iterateComponents(std::invocable<Entity, C&> auto func)
            {
                this->allocator.iterateThroughAllocatedElements(
                    [&](std::size_t idx)
                    {
                        func(
                            this->parent_entities[idx],
                            *reinterpret_cast<C*>(
                                this->owned_storage + sizeof(C) * idx));
                    });
            }

            util::TypeErasedData type_info;

            util::IndexAllocator allocator;
            std::vector<Entity>  parent_entities;
            std::byte*           owned_storage;
        };

        std::unique_ptr<InitalizedData> initalized_data;
    };
} // namespace game::ec