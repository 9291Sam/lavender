#pragma once

#include <concepts>
#include <cstddef>
#include <span>
#include <string_view>
#include <util/misc.hpp>

namespace game
{
    using ComponentTypeId = u8;

    template<class T>
    consteval u8 getComponentId() noexcept = delete;

    template<u8 ID>
    consteval std::size_t getComponentSize() noexcept = delete;

    template<u8 ID>
    consteval std::string_view getComponentName() noexcept = delete;

    template<class T>
    concept Component = requires() {
        { getComponentId<T>() } -> std::same_as<u8>;
    };

    template<Component C>
    struct ComponentBase
    {
        static constinit const u8 Id = getComponentId<C>();

        [[nodiscard]] std::span<const std::byte> asBytes() const
        {
            // NOLINTNEXTLINE
            return {reinterpret_cast<const std::byte*>(this), sizeof(C)};
        }
    };
} // namespace game

#define DECLARE_COMPONENT(NAMESPACE, DECLARATION, TYPE, SIZE) /* NOLINT */     \
    namespace NAMESPACE                                                        \
    {                                                                          \
        DECLARATION TYPE;                                                      \
    }                                                                          \
    template<>                                                                 \
    consteval u8 game::getComponentId<NAMESPACE::TYPE>() noexcept              \
    {                                                                          \
        return static_cast<u8>(__LINE__);                                      \
    }                                                                          \
    template<>                                                                 \
    consteval std::size_t                                                      \
    game::getComponentSize<game::getComponentId<NAMESPACE::TYPE>()>() noexcept \
    {                                                                          \
        return static_cast<std::size_t>(SIZE);                                 \
    }                                                                          \
    template<>                                                                 \
    consteval std::string_view                                                 \
    game::getComponentName<game::getComponentId<NAMESPACE::TYPE>()>() noexcept \
    {                                                                          \
        return #TYPE;                                                          \
    }

#line 0
DECLARE_COMPONENT(game::ec, struct, FooComponent, 1);
DECLARE_COMPONENT(game::ec, struct, BarComponent, 1);
DECLARE_COMPONENT(verdigris, struct, TriangleComponent, 40);
DECLARE_COMPONENT(game, struct, TickComponent, 32);
static constinit const u32 NumberOfGameComponents = __LINE__;
// DECLARE_COMPONENT(game, struct, TickComponent);
#undef DECLARE_COMPONENT
