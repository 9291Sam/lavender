#pragma once

#include <cstddef>
#include <type_traits>

namespace game
{
    namespace detail
    {

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"

        static inline std::size_t type_id_counter = 0; // NOLINT

        template<typename T>
        inline const std::size_t EntityId = type_id_counter++;

        template<class T>
        constexpr std::size_t getIdOfEntity()
        {
            return EntityId<T>;
        }
#pragma clang diagnostic pop
    } // namespace detail

    template<class T>
    struct ComponentBase
    {
        [[nodiscard]] constexpr std::size_t getId() const noexcept
        {
            return detail::getIdOfEntity<T>();
        }
    };

    struct FooComponent : ComponentBase<FooComponent>
    {};

    struct BarComponent : ComponentBase<BarComponent>
    {};

    template<class T>
    concept Component = std::is_base_of_v<ComponentBase<T>, T>;

    static_assert(FooComponent {}.getId() == 0);

    class ECManager
    {
    public:

        // bool doesExititExity();

        // getCombponent(Enttity, ComponentId);
    };
} // namespace game
