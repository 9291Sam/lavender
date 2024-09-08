#include "gfx/renderer.hpp"
#include "util/log.hpp"
#include <cstdlib>

int main()
{
    util::installGlobalLoggerRacy();

    gfx::Renderer renderer {};

    renderer.renderOnThread();

    util::removeGlobalLoggerRacy();

    return EXIT_SUCCESS;
}
