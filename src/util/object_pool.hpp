#pragma once

#include "log.hpp"
#include <boost/pool/object_pool.hpp>
#include <cstddef>
#include <memory>

namespace util
{
    template<class T>
    class ObjectPool
    {
    public:
        struct Deleter
        {
            ObjectPool<T>* self;

            void operator() (T* ptr)
            {
                self->deallocate(ptr);
            }
        };

        using UniqueT = std::unique_ptr<T, Deleter>;
    public:
        ObjectPool(std::size_t allocationIncrement, std::size_t maxElements)
            : pool {allocationIncrement, maxElements}
        {}

        template<class... Args>
        UniqueT allocate(Args&&... args)
        {
            T* const newT = this->pool.malloc();

            if (newT == 0)
            {
                util::panic("Overallocate Pool");
            }

            try
            {
                new (newT) T(std::forward<Args>(args)...);
            }
            catch (...)
            {
                this->pool.free(newT);
                throw;
            }

            return UniqueT {newT, Deleter {this}};
        }

    private:
        void deallocate(T* t)
        {
            this->pool.destroy(t);
        }

        boost::object_pool<T> pool;
    };
} // namespace util