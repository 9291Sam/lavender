#include "game/game.hpp"
#include "util/log.hpp"
#include <functional>
#include <game/ec_manager.hpp>

int main()
{
    util::installGlobalLoggerRacy();

    try
    {
        util::logTrace(
            "{} {} {}",
            game::FooComponent {}.getId(),
            game::BarComponent {}.getId(),
            game::FooComponent {}.getId());

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
