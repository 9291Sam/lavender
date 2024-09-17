#include "game/game.hpp"
#include "util/log.hpp"
#include <functional>
#include <game/ec/entity_component_manager.hpp>

int main()
{
    util::installGlobalLoggerRacy();

    try
    {
        game::Game game {};

        game.run();
    }
    catch (const std::exception& e)
    {
        util::logFatal("Lavender has crashed! | {}", e.what());
    }
    catch (...)
    {
        util::logFatal("Lavender has crashed! | Non Exception Thrown!");
    }

    util::logLog("lavender exited successfully!");

    util::removeGlobalLoggerRacy();

    return EXIT_SUCCESS;
}
