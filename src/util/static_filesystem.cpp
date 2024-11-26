#include "static_filesystem.hpp"
#include "cmrc/cmrc.hpp"
#include "util/log.hpp"
#include "util/misc.hpp"
#include <source_location>
#include <span>
#include <system_error>

CMRC_DECLARE(lav);

namespace staticFilesystem
{
    std::span<const std::byte>
    loadResource(std::string_view resource, std::source_location location)
    {
        try
        {
            std::string ownedString {resource};

            const cmrc::file resourceFile = cmrc::lav::get_filesystem().open(ownedString);

            // NOLINTNEXTLINE
            return {reinterpret_cast<const std::byte*>(resourceFile.begin()), resourceFile.size()};
        }
        catch (const std::system_error& e)
        {
            util::panic<const std::string_view&, const char*>(
                "failed to open virtual resource {} | {}", resource, e.what(), location);
        }

        util::debugBreak();
    }

    std::span<const std::byte> loadShader(std::string_view shader, std::source_location location)
    {
        std::string realResource {"build/src/shaders/"};
        realResource += shader;
        realResource += ".bin";

        return loadResource(realResource, location);
    }
} // namespace staticFilesystem
