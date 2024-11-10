#include "game/game.hpp"
#include "util/log.hpp"
#include "util/misc.hpp"
#include "verdigris/verdigris.hpp"
#include <cassert>
#include <ctti/type_id.hpp>
#include <glm/gtx/hash.hpp>
#include <iostream>
#include <utility>
#include <vector>

int main()
{
    util::installGlobalLoggerRacy();

    try
    {
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
