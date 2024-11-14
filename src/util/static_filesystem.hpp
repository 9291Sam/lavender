#include <span>
#include <string_view>

namespace staticFilesystem
{
    [[nodiscard]] std::span<const std::byte> loadResource(const std::string&);
    [[nodiscard]] std::span<const std::byte> loadShader(std::string_view);

} // namespace staticFilesystem
