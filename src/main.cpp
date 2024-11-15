#include "game/game.hpp"
#include "util/log.hpp"
#include "util/misc.hpp"
#include "util/ranges.hpp"
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
        // std::vector<util::InclusiveRange> ranges {};

        // ranges.push_back({65, 78});
        // ranges.push_back({64, 73});
        // ranges.push_back({65, 78});
        // ranges.push_back({65, 79});

        // for (std::size_t i = 0; i < 12; ++i)
        // {
        //     ranges.push_back({i, i});
        // }

        // ranges.push_back({2365, 2478});

        // ranges.push_back({14, 18});
        // ranges.push_back({15, 22});

        // ranges.push_back({165, 178});

        // ranges.push_back({238, 1283});

        // ranges.push_back({3165, 9378});

        // ranges.push_back({1324, 38418});
        // ranges.push_back({1325, 38422});

        // std::vector<util::InclusiveRange> combined = util::mergeDownRanges(ranges, 3);
        // util::panic("kill");
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
