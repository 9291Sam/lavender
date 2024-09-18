#pragma once

#include "components.hpp"
#include "util/index_allocator.hpp"
#include "util/type_erasure.hpp"

namespace game::ec
{
    class TypeErasedComponentStorage
    {
    public:
        template<Component C>
        TypeErasedComponentStorage(std::size_t componentSize)
        {}

        ~TypeErasedComponentStorage() noexcept;

        TypeErasedComponentStorage(const TypeErasedComponentStorage&) = delete;
        TypeErasedComponentStorage(TypeErasedComponentStorage&&)      = delete;
        TypeErasedComponentStorage&
        operator= (const TypeErasedComponentStorage&) = delete;
        TypeErasedComponentStorage&
        operator= (TypeErasedComponentStorage&&) = delete;



    private:
        util::IndexAllocator                allocator;
        std::optional<util::TypeErasedData> type_info;
    }
} // namespace game::ec