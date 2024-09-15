#pragma once

#include <concepts>
#include <cstddef>
#include <span>
#include <type_traits>
#include <util/misc.hpp>

namespace game
{
    using ComponentId = U8;

    template<class T>
    consteval U8 getComponentId() noexcept = delete;

    template<class T>
    concept Component = requires() {
        { getComponentId<T>() } -> std::same_as<U8>;
    };

    template<Component C>
    struct ComponentBase
    {
        static constinit const U8 Id = getComponentId<C>();

        [[nodiscard]] static consteval U8 getSize()
        {
            return sizeof(C);
        }

        [[nodiscard]] static consteval U8 getAlign()
        {
            return alignof(C);
        }

        [[nodiscard]] std::span<const std::byte> asBytes() const
        {
            // NOLINTNEXTLINE
            return {reinterpret_cast<const std::byte*>(this), sizeof(C)};
        }
    };
} // namespace game

#define DECLARE_COMPONENT(NAMESPACE, DECLARATION, TYPE) /* NOLINT */           \
    namespace NAMESPACE                                                        \
    {                                                                          \
        DECLARATION TYPE;                                                      \
    }                                                                          \
    template<>                                                                 \
    consteval U8 game::getComponentId<NAMESPACE::TYPE>() noexcept              \
    {                                                                          \
        return static_cast<U8>(__LINE__);                                      \
    }

#line 0
DECLARE_COMPONENT(game::ec, struct, FooComponent);
DECLARE_COMPONENT(game::ec, struct, BarComponent);
DECLARE_COMPONENT(game, struct, TickComponent);
DECLARE_COMPONENT(game::render, struct, TriangleComponent);
static constinit const U32 NumberOfGameComponents = __LINE__;

#undef DECLARE_COMPONENT