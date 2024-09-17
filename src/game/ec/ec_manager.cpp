#include "ec_manager.hpp"
#include <type_traits>

namespace game::ec
{
    ECManager::ECManager()
        : component_storage {
              util::Mutex {MuckedComponentStorage(128, getComponentSize<0>())},
              util::Mutex {MuckedComponentStorage(128, getComponentSize<1>())},
              util::Mutex {MuckedComponentStorage(128, getComponentSize<2>())}}
    {}
} // namespace game::ec
