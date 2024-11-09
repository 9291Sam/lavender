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
        // Input data that will lead to ranges [1, 4] and [7, 9]
        std::vector<size_t> points = {1, 2, 3, 4, 7, 8, 9};
        size_t maxDistance = 2; // Allows points within a distance of 2 to be part of the same range
        size_t maxRanges   = 1; // Forces merging of ranges into a single range

        // Expected output: One merged range [1, 9]
        std::vector<std::pair<size_t, size_t>> expectedRanges = {{1, 9}};

        // Run the function
        std::vector<std::pair<size_t, size_t>> result =
            util::combineIntoRanges(points, maxDistance, maxRanges);

        // Verify the result
        assert(result == expectedRanges && "The ranges did not merge as expected.");

        // If we reach here, the test passed
        std::cout << "Test passed: Merged ranges are [1, 9]." << std::endl;

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
