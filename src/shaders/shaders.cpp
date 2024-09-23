#include "shaders.hpp"
#include "cmrc/cmrc.hpp"
#include "util/log.hpp"
#include "util/misc.hpp"
#include <span>

namespace shaders
{
    std::span<const std::byte> load(const std::string& resource) noexcept
    {
        std::string realResource = "build/src/shaders/" + resource + ".bin";

        try
        {
            const cmrc::file resourceFile =
                cmrc::lav::get_filesystem().open(realResource);

            const std::byte* ptr = reinterpret_cast<const std::byte*>(
                resourceFile.begin()); // NOLINT
            const std::size_t len = resourceFile.size();

            return {ptr, len};
        }
        catch (...)
        {
            util::panic("failed to open resource {}", realResource);
        }

        util::debugBreak();
    }
} // namespace shaders
