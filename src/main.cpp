
#include "ecs/entity.hpp"
#include "ecs/entity_component_system_manager.hpp"
#include "ecs/raw_entity.hpp"
#include "game/game.hpp"
#include "util/log.hpp"
#include "util/thread_pool.hpp"
#include "verdigris/verdigris.hpp"

//

// // Threadsafe, efficieint
// class EntityManager
// {
// public:

//     [[nodiscard]] u32 createEntity() const;
//     void              destroyEntity(u32) const;

//     [[nodiscard]] bool unGuardedIsEntityAlive() const;
//     std::shared_mutex& getGuardForEntity(u32 entity, std::invocable<> auto func) const {}

// private:
//     std::atomic<u32> next_valid_entity_id;
// };

// Entity Id Allocator
// Entity -> Component Map
// Component Data Storage

class ConcreteEntity : DERIVE_INHERENT_ENTITY(ConcreteEntity, entity)
{
public:
    explicit ConcreteEntity(int health_ = 100)
        : name {"Unknown"}
        , health {health_}
        , entity {ecs::createEntity()}
    {}

    [[nodiscard]] int getHealth() const
    {
        return this->health;
    }
    void setHealth(int health_)
    {
        this->health = health_;
    }

    [[nodiscard]] std::string_view getName() const
    {
        return this->name;
    }
    void setName(std::string_view newName)
    {
        this->name = newName;
    }

private:
    std::string       name;
    int               health;
    ecs::UniqueEntity entity;
};

int main()
{
    util::installGlobalLoggerRacy();
    util::installGlobalThreadPoolRacy();
    ecs::installGlobalECSManagerRacy();

    try

    {
        ConcreteEntity c {};

        int f = std::bit_cast<long long>(c.offset());

        util::logLog(
            "starting lavender {}.{}.{}.{}{} {}",
            LAVENDER_VERSION_MAJOR,
            LAVENDER_VERSION_MINOR,
            LAVENDER_VERSION_PATCH,
            LAVENDER_VERSION_TWEAK,
            util::isDebugBuild() ? " | Debug Build" : "",
            f);

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

    ecs::removeGlobalECSManagerRacy();
    util::removeGlobalThreadPoolRacy();
    util::removeGlobalLoggerRacy();

    return EXIT_SUCCESS;
}
