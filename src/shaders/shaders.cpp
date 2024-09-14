#include "shaders.hpp"
#include "cmrc/cmrc.hpp"
#include <span>

namespace shaders
{
    std::span<const std::byte> load(const std::string& resource) noexcept
    {
        const cmrc::file resourceFile =
            cmrc::lav::get_filesystem().open(resource);

        const std::byte* ptr =
            reinterpret_cast<const std::byte*>(resourceFile.begin()); // NOLINT
        const std::size_t len = resourceFile.size();

        return {ptr, len};
    }
} // namespace shaders
