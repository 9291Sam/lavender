#include <source_location>
#include <span>
#include <string_view>

namespace staticFilesystem
{
    [[nodiscard]] std::span<const std::byte>
        loadResource(std::string_view, std::source_location = std::source_location::current());
    [[nodiscard]] std::span<const std::byte>
        loadShader(std::string_view, std::source_location = std::source_location::current());

} // namespace staticFilesystem
