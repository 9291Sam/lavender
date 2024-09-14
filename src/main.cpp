#include "game/game.hpp"
#include "util/log.hpp"
#include <functional>

int main()
{
    util::installGlobalLoggerRacy();

    try
    {
        util::logTrace(
            "{} {} {}", type_id<int>, type_id<double>, type_id<float>);
        util::logTrace(
            "{} {} {}", type_id<int>, type_id<double>, type_id<float>);
        util::logTrace(
            "{} {} {}", type_id<int>, type_id<double>, type_id<float>);

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
