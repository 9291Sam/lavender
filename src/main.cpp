#include "util/log.hpp"
#include <cstdlib>

int main()
{
    util::installGlobalLoggerRacy();

    for (int i = 0; i < 10000; ++i)
    {
        util::logTrace("Hello, World!");
    }

    util::removeGlobalLoggerRacy();

    return EXIT_SUCCESS;
}
