#pragma once

#include "log.hpp"
#include "misc.hpp"
#include <vector>

namespace util
{
    struct InclusiveRange
    {
        std::size_t start;
        std::size_t end;

        [[nodiscard]] std::size_t size() const
        {
            return end - start + 1;
        }
    };

    std::vector<InclusiveRange> mergeAndSortOverlappingRanges(std::vector<InclusiveRange>);

    std::vector<InclusiveRange>
    mergeDownRanges(std::vector<InclusiveRange>, std::size_t numberOfRanges);
} // namespace util