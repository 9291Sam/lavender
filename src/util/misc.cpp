#include "misc.hpp"
#include "util/log.hpp"
#include <cmath>
#include <cstdio>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <tuple>

namespace util
{
    [[noreturn]] void debugBreak(std::source_location l)
    {
        std::ignore = std::fputs("util::debugBreak()\n", stderr);
        util::logFatal<>("util::DebugBreak", l);

        std::this_thread::sleep_for(std::chrono::milliseconds {250});

        if constexpr (isDebugBuild())
        {
#ifdef _MSC_VER
            __debugbreak();
#elif defined(__GNUC__) || defined(__clang__)
            __builtin_trap();
#else
#error Unsupported compiler
#endif
        }

        throw std::runtime_error {"util::debugBreak()"};
    }

    std::vector<std::pair<size_t, size_t>>
    combineIntoRanges(std::vector<size_t> points, size_t maxDistance, size_t maxRanges)
    {
        if (points.empty())
        {
            return {};
        }

        // Sort the points
        std::sort(points.begin(), points.end());

        std::vector<std::pair<size_t, size_t>> ranges;
        ranges.reserve(points.size());

        size_t start = points[0];
        size_t end   = start;

        // Create initial ranges based on maxDistance
        for (size_t i = 1; i < points.size(); ++i)
        {
            size_t point = points[i];
            if (point - end <= maxDistance)
            {
                end = point;
            }
            else
            {
                ranges.emplace_back(start, end);
                start = point;
                end   = point;
            }
        }

        // Push the last range
        ranges.emplace_back(start, end);

        // If the number of ranges exceeds maxRanges, merge them
        if (ranges.size() > maxRanges)
        {
            // Priority queue to keep track of gaps between ranges
            using Gap = std::pair<size_t, size_t>; // (gap size, index in ranges)
            std::priority_queue<Gap, std::vector<Gap>, std::greater<>> minHeap;

            // Calculate initial gaps between adjacent ranges
            for (size_t i = 0; i < ranges.size() - 1; ++i)
            {
                size_t gap = ranges[i + 1].first - ranges[i].second;
                minHeap.emplace(gap, i);
            }

            // Merge until we reach maxRanges
            while (ranges.size() > maxRanges)
            {
                // Get the smallest gap and corresponding index
                auto [minGap, index] = minHeap.top();
                minHeap.pop();

                // Merge the two ranges at 'index' and 'index + 1'
                ranges[index].second = ranges[index + 1].second;
                ranges.erase(
                    ranges.begin() + static_cast<decltype(ranges)::difference_type>(index) + 1);

                // Remove invalidated gaps and add the updated gaps in the heap
                // Reinserted gaps will now include only the adjacent pairs affected by the merge
                while (!minHeap.empty()
                       && (minHeap.top().second >= ranges.size() - 1
                           || minHeap.top().second == index - 1 || minHeap.top().second == index))
                {
                    minHeap.pop();
                }

                // Update the gap at the current index if there’s a next pair
                if (index < ranges.size() - 1)
                {
                    size_t newGap = ranges[index + 1].first - ranges[index].second;
                    minHeap.emplace(newGap, index);
                }

                // Update the gap at the previous index if there's a previous pair
                if (index > 0)
                {
                    size_t prevGap = ranges[index].first - ranges[index - 1].second;
                    minHeap.emplace(prevGap, index - 1);
                }
            }
        }

        return ranges;
    }

    namespace
    {
        const std::array<std::string_view, 11>& getSuffixes(SuffixType t)
        {
            static std::array<std::string_view, 11> full {
                "B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB", "RiB", "QiB"};

            static std::array<std::string_view, 11> small {
                "B", "K", "M", "G", "T", "P", "E", "Z", "Y", "R", "Q"};

            static std::array<std::string_view, 11> null {};

            switch (t)
            {
            case SuffixType::Full:
                return full;
            case SuffixType::Short:
                return small;
            default:
                return null;
            }
        }
    } // namespace

    std::string bytesAsSiNamed(long double bytes, SuffixType type)
    {
        const long double unit = 1024.0;

        switch (std::fpclassify(bytes))
        {
        case FP_INFINITE:
            return "Infinity Bytes";
        case FP_NAN:
            return "NaN Bytes";
        case FP_ZERO:
            return "0 B";
        default:
            return "unknown";
        case FP_NORMAL:
            [[fallthrough]];
        case FP_SUBNORMAL:
        };

        const long double base  = std::log10(bytes) / std::log10(unit);
        const long double value = std::pow(unit, base - std::floor(base));
        std::string       prefix {std::format("{:.3}", value)};

        if (std::string_view {prefix}.ends_with(".0"))
        {
            prefix.pop_back();
            prefix.pop_back();
        }

        const std::string_view suffix =
            getSuffixes(type).at(static_cast<std::size_t>(std::floor(base)));

        prefix.append_range(" ");
        prefix.append_range(suffix);

        return prefix;
    }

} // namespace util
