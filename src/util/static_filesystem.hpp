#include <span>
#include <string_view>

namespace staticFilesystem
{
    [[nodiscard]] std::span<const std::byte> loadResource(std::string_view);
    [[nodiscard]] std::span<const std::byte> loadShader(std::string_view);

} // namespace staticFilesystem
