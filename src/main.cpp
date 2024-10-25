#include "game/game.hpp"
#include "util/log.hpp"
#include "verdigris/verdigris.hpp"
#include <ctti/type_id.hpp>
#include <glm/gtx/hash.hpp>

std::vector<std::vector<int>>
Bresenham3D(int x1, int y1, int z1, int x2, int y2, int z2)
{
    std::vector<std::vector<int>> ListOfPoints;
    ListOfPoints.push_back({x1, y1, z1});
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int dz = abs(z2 - z1);
    int xs;
    int ys;
    int zs;
    if (x2 > x1)
    {
        xs = 1;
    }
    else
    {
        xs = -1;
    }
    if (y2 > y1)
    {
        ys = 1;
    }
    else
    {
        ys = -1;
    }
    if (z2 > z1)
    {
        zs = 1;
    }
    else
    {
        zs = -1;
    }

    // Driving axis is X-axis"
    if (dx >= dy && dx >= dz)
    {
        int p1 = 2 * dy - dx;
        int p2 = 2 * dz - dx;
        while (x1 != x2)
        {
            x1 += xs;
            if (p1 >= 0)
            {
                y1 += ys;
                p1 -= 2 * dx;
            }
            if (p2 >= 0)
            {
                z1 += zs;
                p2 -= 2 * dx;
            }
            p1 += 2 * dy;
            p2 += 2 * dz;
            ListOfPoints.push_back({x1, y1, z1});
        }

        // Driving axis is Y-axis"
    }
    else if (dy >= dx && dy >= dz)
    {
        int p1 = 2 * dx - dy;
        int p2 = 2 * dz - dy;
        while (y1 != y2)
        {
            y1 += ys;
            if (p1 >= 0)
            {
                x1 += xs;
                p1 -= 2 * dy;
            }
            if (p2 >= 0)
            {
                z1 += zs;
                p2 -= 2 * dy;
            }
            p1 += 2 * dx;
            p2 += 2 * dz;
            ListOfPoints.push_back({x1, y1, z1});
        }

        // Driving axis is Z-axis"
    }
    else
    {
        int p1 = 2 * dy - dz;
        int p2 = 2 * dx - dz;
        while (z1 != z2)
        {
            z1 += zs;
            if (p1 >= 0)
            {
                y1 += ys;
                p1 -= 2 * dz;
            }
            if (p2 >= 0)
            {
                x1 += xs;
                p2 -= 2 * dz;
            }
            p1 += 2 * dy;
            p2 += 2 * dx;
            ListOfPoints.push_back({x1, y1, z1});
        }
    }
    return ListOfPoints;
}

std::vector<std::vector<int>> Bresenham3D2(
    const int x1,
    const int y1,
    const int z1,
    const int x2,
    const int y2,
    const int z2)
{
    std::vector<std::vector<int>> ListOfPoints;
    ListOfPoints.push_back({x1, y1, z1});

    const glm::ivec3 start {x1, y1, z1};
    const glm::ivec3 end {x2, y2, z2};

    const glm::ivec3 dir = glm::abs(end - start);
    const glm::ivec3 step {
        end.x > start.x ? 1 : -1,
        end.y > start.y ? 1 : -1,
        end.z > start.z ? 1 : -1,
    }; // TODO: try 0?

    enum DrivingAxis
    {
        X = 0,
        Y = 1,
        Z = 2
    };

    DrivingAxis drivingAxis {};
    DrivingAxis errorAxis0 {};
    DrivingAxis errorAxis1 {};
    if (dir.x >= dir.y && dir.x >= dir.z)
    {
        drivingAxis = DrivingAxis::X;
        errorAxis0  = DrivingAxis::Y;
        errorAxis1  = DrivingAxis::Z;
    }
    else if (dir.y >= dir.x && dir.y >= dir.z)
    {
        drivingAxis = DrivingAxis::Y;
        errorAxis0  = DrivingAxis::X;
        errorAxis1  = DrivingAxis::Z;
    }
    else
    {
        drivingAxis = DrivingAxis::Z;
        errorAxis0  = DrivingAxis::Y;
        errorAxis1  = DrivingAxis::X;
    }

    glm::ivec3 traverse {start};

    int err0 = 2 * dir[errorAxis0] - dir[drivingAxis];
    int err1 = 2 * dir[errorAxis1] - dir[drivingAxis];

    while (traverse[drivingAxis] != end[drivingAxis])
    {
        traverse[drivingAxis] += step[drivingAxis];

        if (err0 >= 0)
        {
            traverse[errorAxis0] += step[errorAxis0];
            err0 -= 2 * dir[drivingAxis];
        }

        if (err1 >= 0)
        {
            traverse[errorAxis1] += step[errorAxis0];
            err1 -= 2 * dir[drivingAxis];
        }

        err0 += 2 * dir[errorAxis0];
        err1 += 2 * dir[errorAxis1];

        ListOfPoints.push_back({traverse.x, traverse.y, traverse.z});
    }

    return ListOfPoints;
}

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
