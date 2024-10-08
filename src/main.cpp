#include "game/game.hpp"
#include "util/log.hpp"
#include "verdigris/verdigris.hpp"
#include <ctti/type_id.hpp>
#include <limits>

int main()
{
    util::installGlobalLoggerRacy();

    try
    {
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
