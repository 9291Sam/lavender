#include "game/game.hpp"
#include "util/log.hpp"
#include "verdigris/verdigris.hpp"
#include <ctti/type_id.hpp>
#include <limits>

u32 positionToNumber(const glm::uvec3& brick_local_position)
{
    return brick_local_position.x + brick_local_position.y * 8
         + brick_local_position.z * 8 * 8;
}

glm::uvec3 numberToPosition(u32 brick_local_number)
{
    u32 z = brick_local_number / (8 * 8);
    u32 y = (brick_local_number % (8 * 8)) / 8;
    u32 x = brick_local_number % 8;
    return {x, y, z};
}

void testPositionToNumberAndBack()
{
    for (u32 x = 0; x < 8; ++x)
    {
        for (u32 y = 0; y < 8; ++y)
        {
            for (u32 z = 0; z < 8; ++z)
            {
                glm::uvec3 original_position = {x, y, z};
                u32 brick_local_number = positionToNumber(original_position);
                glm::uvec3 recovered_position =
                    numberToPosition(brick_local_number);

                // Check that the recovered position matches the original
                util::assertFatal(original_position == recovered_position, "");
            }
        }
    }
}

int main()
{
    util::installGlobalLoggerRacy();

    try
    {
        testPositionToNumberAndBack();
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
