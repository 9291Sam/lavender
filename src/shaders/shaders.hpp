
#include <cmrc/cmrc.hpp>
#include <span>

CMRC_DECLARE(lav);

namespace shaders
{
    [[nodiscard]] std::span<const std::byte> load(const std::string& resource) noexcept;

} // namespace shaders
