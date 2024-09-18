#pragma once

#include "util/misc.hpp"
#include <type_traits>

namespace util
{
    struct TypeErasedData
    {
        template<class T>
            requires std::is_default_constructible_v<T>
                      && std::is_nothrow_destructible_v<T>
                      && std::is_nothrow_move_constructible_v<T>
                      && std::is_copy_constructible_v<T>
        explicit TypeErasedData(util::ZSTTypeWrapper<T>)
            : constructor {[](void* self)
                           {
                               return static_cast<void*>(new (self) T {});
                           }}
            , destructor {[](void* self) noexcept
                          {
                              reinterpret_cast<T*>(self)->~T();
                          }}
            , move {[](void* to, void* from) noexcept
                    {
                        return static_cast<void*>(new (to) T {
                            static_cast<T&&>(*reinterpret_cast<T*>(from))});
                    }}
            , copy {[](void* to, void* from)
                    {
                        return static_cast<void*>(new (to) T {
                            static_cast<T&>(*reinterpret_cast<T*>(from))});
                    }}
            , size {sizeof(T)}
            , align {alignof(T)}
        {}

        void* (*constructor)(void*);
        void (*destructor)(void*) noexcept;
        void* (*move)(void*, void*) noexcept;
        void* (*copy)(void*, void*);
        std::size_t size;
        std::size_t align;
    };

} // namespace util