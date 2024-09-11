#include "gfx/renderer.hpp"
#include "util/log.hpp"
#include <cstdlib>
#include <exception>

int main()
{
    util::installGlobalLoggerRacy();

    try
    {
        gfx::Renderer renderer {};

        renderer.renderOnThread();
    }
    catch (const std::exception& e)
    {
        util::logFatal("Lavender has crashed! | {}", e.what());
    }
    catch (...)
    {
        util::logFatal("Lavender has crashed! | Non Exception Thrown!");
    }

    util::removeGlobalLoggerRacy();

    return EXIT_SUCCESS;
}
