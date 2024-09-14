#pragma once

#include <concepts>
#include <cstddef>
#include <type_traits>

namespace game
{

    template<class T>
    consteval std::size_t getComponentId() noexcept = delete;

    template<class T>
    concept Component =
        std::same_as<std::size_t, decltype(getComponentId<T>())>;

    template<Component T>
    struct ComponentBase
    {
        static constexpr std::size_t Id = getComponentId<T>();
    };
} // namespace game

#define DECLARE_COMPONENT(NAMESPACE, DECLARATION, TYPE) /* NOLINT */           \
    namespace NAMESPACE                                                        \
    {                                                                          \
        DECLARATION TYPE;                                                      \
    }                                                                          \
    template<>                                                                 \
    consteval std::size_t game::getComponentId<NAMESPACE::TYPE>() noexcept     \
    {                                                                          \
        return __LINE__;                                                       \
    }

#line 0
DECLARE_COMPONENT(game, struct, FooComponent);
DECLARE_COMPONENT(game, struct, BarComponent);

#undef DECLARE_COMPONENT

namespace game
{
    struct FooComponent : ComponentBase<FooComponent>
    {};

    struct BarComponent : ComponentBase<BarComponent>
    {};

    static_assert(FooComponent::Id == 0);
    static_assert(BarComponent::Id == 1);

    class ECManager
    {
    public:
    };
} // namespace game
