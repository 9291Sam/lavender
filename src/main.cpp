#include "ecs/entity.hpp"
#include "ecs/entity_component_system_manager.hpp"
#include "ecs/raw_entity.hpp"
#include "game/game.hpp"
#include "util/log.hpp"
#include "util/misc.hpp"
#include "util/ranges.hpp"
#include "verdigris/verdigris.hpp"
#include <algorithm>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <cassert>
#include <concepts>
#include <ctti/type_id.hpp>
#include <glm/gtx/hash.hpp>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

// // Forward declaration of EntityManager
// class EntityManager;

// class UniqueEntity
// {
// public:
//     UniqueEntity() = delete;
//     ~UniqueEntity();

//     UniqueEntity(const UniqueEntity&)             = delete;
//     UniqueEntity(UniqueEntity&&)                  = default;
//     UniqueEntity& operator= (const UniqueEntity&) = delete;
//     UniqueEntity& operator= (UniqueEntity&&)      = default;

//     [[nodiscard]] const EntityManager* getOwner() const
//     {
//         return this->owner;
//     }

//     [[nodiscard]] uint64_t getId() const
//     {
//         return this->id;
//     }

// public:
//     friend class EntityManager;

//     UniqueEntity(EntityManager* owner, uint64_t id)
//         : owner(owner)
//         , id(id)
//     {}

//     // These methods are defined after EntityManager is fully declared
//     template<typename Component, typename... Args>
//     void addComponent(Args&&... args);

//     template<typename Component>
//     void removeComponent();

//     template<typename Component>
//     Component& getComponent();

//     template<typename Component>
//     const Component& getComponent() const;

//     EntityManager* owner;
//     uint64_t       id;
// };

// // Base class for inherently allocated entities
// struct InherentEntity
// {
//     virtual ~InherentEntity() = default;

//     InherentEntity(const InherentEntity&) = delete;
//     InherentEntity(InherentEntity&&)      = delete;

//     // Get the UniqueEntity and invoke component functions on it
//     [[nodiscard]] virtual UniqueEntity& getEntity() const = 0;

//     // Delegate the component management to the UniqueEntity's methods
//     template<typename Component, typename... Args>
//     void addComponent(Args&&... args)
//     {
//         getEntity().addComponent<Component>(std::forward<Args>(args)...);
//     }

//     template<typename Component>
//     void removeComponent()
//     {
//         getEntity().removeComponent<Component>();
//     }

//     template<typename Component>
//     Component& getComponent()
//     {
//         return getEntity().getComponent<Component>();
//     }

//     template<typename Component>
//     const Component& getComponent() const
//     {
//         return getEntity().getComponent<Component>();
//     }
// };

// // Component storage
// class ComponentStorageBase
// {
// public:
//     virtual ~ComponentStorageBase() = default;
// };

// template<typename Component>
// class ComponentStorage : public ComponentStorageBase
// {
// public:
//     void add(uint64_t entityId, Component component)
//     {
//         if (entityIndexMap.find(entityId) != entityIndexMap.end())
//         {
//             throw std::runtime_error("Component already exists for this entity!");
//         }
//         size_t index = components.size();
//         components.push_back(std::move(component));
//         entityIndexMap[entityId] = index;
//     }

//     void remove(uint64_t entityId)
//     {
//         auto it = entityIndexMap.find(entityId);
//         if (it == entityIndexMap.end())
//         {
//             throw std::runtime_error("Component does not exist for this entity!");
//         }
//         size_t index = it->second;
//         if (index != components.size() - 1)
//         {
//             std::swap(components[index], components.back());
//             auto swappedEntity = std::find_if(
//                 entityIndexMap.begin(),
//                 entityIndexMap.end(),
//                 [index](const auto& pair)
//                 {
//                     return pair.second == index;
//                 });
//             swappedEntity->second = index;
//         }
//         components.pop_back();
//         entityIndexMap.erase(it);
//     }

//     Component& get(uint64_t entityId)
//     {
//         auto it = entityIndexMap.find(entityId);
//         if (it == entityIndexMap.end())
//         {
//             throw std::runtime_error("Component does not exist for this entity!");
//         }
//         return components[it->second];
//     }

//     const Component& get(uint64_t entityId) const
//     {
//         auto it = entityIndexMap.find(entityId);
//         if (it == entityIndexMap.end())
//         {
//             throw std::runtime_error("Component does not exist for this entity!");
//         }
//         return components[it->second];
//     }

// private:
//     std::vector<Component>               components;
//     std::unordered_map<uint64_t, size_t> entityIndexMap;
// }; // Base class for entity storage
// class EntityStorageBase
// {
// public:
//     virtual ~EntityStorageBase() = default;
// };

// // Entity Storage to hold entities of a specific type
// template<typename T>
// class EntityStorage : public EntityStorageBase
// {
// public:
//     T* createEntity(UniqueEntity entity, auto&&... args)
//     {
//         entities.emplace_back(std::move(entity), std::forward<decltype(args)>(args)...);
//         return &entities.back();
//     }

//     std::vector<T>& getEntities()
//     {
//         return entities;
//     }
//     const std::vector<T>& getEntities() const
//     {
//         return entities;
//     }

// private:
//     std::vector<T> entities;
// };

// // Entity Manager
// class EntityManager
// {
// public:
//     template<typename T>
//     struct EntityDeleter;

//     template<typename T>
//     using ManagedEntityPtr = std::unique_ptr<T, EntityDeleter<T>>;

//     template<typename T, typename... Args>
//     ManagedEntityPtr<T> createInherentEntity(Args&&... args)
//     {
//         static_assert(std::is_base_of_v<InherentEntity, T>, "T must inherit from
//         InherentEntity!"); static_assert(
//             std::is_constructible_v<T, UniqueEntity, Args...>,
//             "T must be constructible from UniqueEntity and Args!");

//         auto id     = allocateEntityId();
//         auto entity = UniqueEntity {this, id};

//         auto& storage     = getStorage<T>();
//         T*    ptr         = storage.createEntity(std::move(entity), std::forward<Args>(args)...);
//         entityIndices[id] = reinterpret_cast<void*>(ptr);

//         return ManagedEntityPtr<T>(ptr, EntityDeleter<T> {this});
//     }

//     template<typename Component, typename... Args>
//     void addComponent(uint64_t entityId, Args&&... args)
//     {
//         getOrCreateComponentStorage<Component>().add(
//             entityId, Component(std::forward<Args>(args)...));
//     }

//     template<typename Component>
//     void removeComponent(uint64_t entityId)
//     {
//         getOrCreateComponentStorage<Component>().remove(entityId);
//     }

//     template<typename Component>
//     Component& getComponent(uint64_t entityId)
//     {
//         return getOrCreateComponentStorage<Component>().get(entityId);
//     }

//     template<typename Component>
//     const Component& getComponent(uint64_t entityId) const
//     {
//         return getOrCreateComponentStorage<Component>().get(entityId);
//     }

//     void free(const UniqueEntity* entity)
//     {
//         auto it = entityIndices.find(entity->getId());
//         if (it != entityIndices.end())
//         {
//             entityIndices.erase(it);
//         }
//     }

//     template<typename T>
//     EntityStorage<T>& getStorage()
//     {
//         auto it = storages.find(typeid(T));
//         if (it == storages.end())
//         {
//             // Create a new storage if it doesn't exist
//             auto storage        = std::make_unique<EntityStorage<T>>();
//             storages[typeid(T)] = std::move(storage);
//         }
//         return *reinterpret_cast<EntityStorage<T>*>(storages[typeid(T)].get());
//     }

//     template<typename T>
//     struct EntityDeleter
//     {
//         EntityManager* manager;
//         void           operator() (T* entity) const
//         {
//             manager->free(&entity->getEntity());
//         }
//     };

//     uint64_t allocateEntityId()
//     {
//         return nextEntityId++;
//     }

//     template<typename Component>
//     ComponentStorage<Component>& getOrCreateComponentStorage()
//     {
//         auto it = componentStorages.find(typeid(Component));
//         if (it == componentStorages.end())
//         {
//             componentStorages[typeid(Component)] =
//             std::make_unique<ComponentStorage<Component>>();
//         }
//         return *reinterpret_cast<ComponentStorage<Component>*>(
//             componentStorages[typeid(Component)].get());
//     }

// private:
//     uint64_t                                                                   nextEntityId = 0;
//     std::unordered_map<std::type_index, std::unique_ptr<ComponentStorageBase>> componentStorages;
//     std::unordered_map<std::type_index, std::unique_ptr<EntityStorageBase>>
//                                         storages; // Added for polymorphic storage handling
//     std::unordered_map<uint64_t, void*> entityIndices;
// };

// UniqueEntity::~UniqueEntity()
// {
//     this->owner->free(this);
// }

// template<typename Component, typename... Args>
// void UniqueEntity::addComponent(Args&&... args)
// {
//     if (owner)
//     {
//         owner->addComponent<Component>(id, std::forward<Args>(args)...);
//     }
// }

// template<typename Component>
// void UniqueEntity::removeComponent()
// {
//     if (owner)
//     {
//         owner->removeComponent<Component>(id);
//     }
// }

// template<typename Component>
// Component& UniqueEntity::getComponent()
// {
//     if (owner)
//     {
//         return owner->getComponent<Component>(id);
//     }
//     throw std::runtime_error("Owner is not valid!");
// }

// template<typename Component>
// const Component& UniqueEntity::getComponent() const
// {
//     if (owner)
//     {
//         return owner->getComponent<Component>(id);
//     }
//     throw std::runtime_error("Owner is not valid!");
// }

// A concrete entity type that inherits from InherentEntity
class ConcreteEntity : DERIVE_INHERENT_ENTITY(ConcreteEntity, entity_)
{
public:

    ConcreteEntity(ecs::RawEntity entity)
        : entity_(std::move(entity))
        , health_(100)
        , name_("Unknown")
    {}

    // Some data related to the concrete entity
    int getHealth() const
    {
        return health_;
    }
    void setHealth(int health)
    {
        health_ = health;
    }

    std::string getName() const
    {
        return name_;
    }
    void setName(const std::string& name)
    {
        name_ = name;
    }

private:
    int            health_;
    ecs::RawEntity entity_;
    std::string    name_;
};

// template<typename T>
// using ManagedEntityPtr = EntityManager::ManagedEntityPtr<T>;

int main()
{
    util::installGlobalLoggerRacy();
    ecs::installGlobalECSManagerRacy();

    try
    {
        ecs::RawEntity e = ecs::getGlobalECSManager()->createEntity();
        ecs::getGlobalECSManager()->addComponent(e, int {4});

        util::logTrace("{} {}", ecs::getGlobalECSManager()->removeComponent<int>(e), "oisdf");

        // ecs::UniqueEntity e {};

        ecs::ManagedEntityPtr<ConcreteEntity> entity =
            ecs::getGlobalECSManager()->allocateInherentEntity<ConcreteEntity>();

        entity->addComponent(int {4});
        util::logTrace("{} {}", ecs::getGlobalECSManager()->removeComponent<int>(*entity), "oisdf");

        // // Add components to the entity via member functions
        // entity->addComponent<int>(42);
        // entity->addComponent<std::string>("Hello ECS");

        // // Add some data to the ConcreteEntity
        // entity->setHealth(75);
        // entity->setName("PlayerOne");

        // // Log component values
        // util::logLog("Component int: {}", entity->getComponent<int>());
        // util::logLog("Component std::string: {}", entity->getComponent<std::string>());

        // // Log entity's own data
        // util::logLog("Entity health: {}", entity->getHealth());
        // util::logLog("Entity name: {}", entity->getName());

        util::logLog(
            "starting lavender {}.{}.{}.{}{}",
            LAVENDER_VERSION_MAJOR,
            LAVENDER_VERSION_MINOR,
            LAVENDER_VERSION_PATCH,
            LAVENDER_VERSION_TWEAK,
            util::isDebugBuild() ? " | Debug Build" : "");

        game::Game game {};

        game.loadGameState<verdigris::Verdigris>();

        game.run();
    }
    catch (const std::exception& e)
    {
        util::logFatal("Lavender has crashed! | {} {}", e.what(), typeid(e).name());
    }
    catch (...)
    {
        util::logFatal("Lavender has crashed! | Non Exception Thrown!");
    }

    util::logLog("lavender exited successfully!");

    util::removeGlobalLoggerRacy();

    return EXIT_SUCCESS;
}
