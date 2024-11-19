#include "ecs/entity.hpp"
#include "ecs/entity_component_system_manager.hpp"
#include "ecs/raw_entity.hpp"
#include "game/game.hpp"
#include "util/log.hpp"
#include "util/misc.hpp"
#include "verdigris/verdigris.hpp"
#include <boost/unordered/concurrent_flat_map.hpp>
#include <cassert>
#include <ctti/type_id.hpp>
#include <glm/gtx/hash.hpp>
#include <memory>
#include <string_view>
#include <utility>

class ConcreteEntity : DERIVE_INHERENT_ENTITY(ConcreteEntity, entity)
{
public:
    explicit ConcreteEntity(ecs::UniqueEntity entity_, int health_ = 100)
        : name {"Unknown"}
        , health {health_}
        , entity {std::move(entity_)}
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
    ecs::installGlobalECSManagerRacy();

    try
    {
        util::logLog(
            "starting lavender {}.{}.{}.{}{}",
            LAVENDER_VERSION_MAJOR,
            LAVENDER_VERSION_MINOR,
            LAVENDER_VERSION_PATCH,
            LAVENDER_VERSION_TWEAK,
            util::isDebugBuild() ? " | Debug Build" : "");

        ecs::ManagedEntityPtr<ConcreteEntity> entity =
            ecs::allocateInherentEntity<ConcreteEntity>(14);

        util::logLog("Entity health: {}", entity->getHealth());

        entity->addComponent<int>(42);
        entity->addComponent<std::string>("Hello");

        entity->setHealth(75);
        entity->setName("Player");

        util::logLog("Component int: {}", entity->getComponent<int>());
        util::logLog("Component std::string: {}", entity->getComponent<std::string>());

        util::logLog("Entity health: {}", entity->getHealth());
        util::logLog("Entity name: {}", entity->getName());

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
    util::removeGlobalLoggerRacy();

    return EXIT_SUCCESS;
}
