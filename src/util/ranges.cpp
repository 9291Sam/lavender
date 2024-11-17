#include "ranges.hpp"
#include "util/log.hpp"
#include <algorithm>
#include <boost/sort/block_indirect_sort/block_indirect_sort.hpp>
#include <limits>
#include <optional>
#include <ranges>
#include <vector>

namespace util
{
    InclusiveRange makeInvalidIterant()
    {
        return InclusiveRange {~0UZ, ~0UZ};
    }

    bool isIterantInvalid(InclusiveRange r)
    {
        return r.start == ~0UZ && r.end == ~0UZ;
    }

    // std::string formatVectorRanges(const std::vector<InclusiveRange>& r)
    // {
    //     std::string result {};

    //     for (auto rr : r)
    //     {
    //         result += std::format("[{}, {}], ", rr.start, rr.end);
    //     }

    //     return result;
    // }

    std::vector<InclusiveRange> mergeAndSortOverlappingRanges(std::vector<InclusiveRange> ranges)
    {
        if (ranges.size() <= 1)
        {
            return ranges;
        }

        boost::sort::block_indirect_sort(
            ranges.begin(),
            ranges.end(),
            [](const InclusiveRange& l, const InclusiveRange& r)
            {
                return l.start < r.start;
            });

        std::size_t numberOfValidRanges = ranges.size();

        for (std::size_t i = 0; i < ranges.size(); ++i)
        {
            InclusiveRange& base = ranges[i];

            if (isIterantInvalid(base))
            {
                continue;
            }

            for (std::size_t j = i + 1; j < ranges.size(); ++j)
            {
                InclusiveRange& iterant = ranges[j];

                if (isIterantInvalid(iterant))
                {
                    continue;
                }

                if (iterant.start <= base.end || base.end == iterant.start - 1)
                {
                    base.end = iterant.end;
                    iterant  = makeInvalidIterant();
                    numberOfValidRanges -= 1;
                }
                else
                {
                    break;
                }
            }
        }

        std::vector<InclusiveRange> output {};
        output.reserve(numberOfValidRanges);

        for (const InclusiveRange& r : ranges)
        {
            if (!isIterantInvalid(r))
            {
                output.push_back(r);
            }
        }

        return output;
    }

    std::vector<InclusiveRange>
    mergeDownRanges(std::vector<InclusiveRange> inputRanges, std::size_t numberOfRanges)
    {
        std::vector<InclusiveRange> mergedSortedRanges =
            mergeAndSortOverlappingRanges(std::move(inputRanges));

        if (mergedSortedRanges.size() <= numberOfRanges)
        {
            return mergedSortedRanges;
        }

        struct DeltaInformation
        {
            std::size_t referent_base_index; // [a, b] this a
            std::size_t distance_between;    // gap
        };

        std::vector<DeltaInformation> deltas {};
        deltas.reserve(mergedSortedRanges.size() - 1);

        for (std::size_t i = 0; i < mergedSortedRanges.size() - 1; ++i)
        {
            deltas.push_back(DeltaInformation {
                .referent_base_index {i},
                .distance_between {mergedSortedRanges[i + 1].start - mergedSortedRanges[i].end}});
        }

        std::ranges::sort(
            deltas,
            [](const DeltaInformation& l, const DeltaInformation& r)
            {
                return l.distance_between < r.distance_between;
            });
        // discard all of the deltas that are higher than we care about
        deltas.resize(deltas.size() - numberOfRanges);

        // {
        //     std::string msg {};

        //     for (auto& d : deltas)
        //     {
        //         msg += std::format("{{#{}, ~{}}}, ", d.referent_base_index, d.distance_between);
        //     }

        //     util::logTrace("sorted deltas deltas {}", msg);
        // }

        std::vector<std::size_t> nonMonotonicBuckets {};
        nonMonotonicBuckets.resize(
            mergedSortedRanges.size(), std::numeric_limits<std::size_t>::max());

        // {
        //     std::string msg {};

        //     for (auto& b : nonMonotonicBuckets)
        //     {
        //         msg += std::format("{{b{}}}, ", b);
        //     }

        //     util::logTrace("pre nonMonotonicBuckets {}", msg);
        // }

        std::size_t nextBucket = 0;

        for (const DeltaInformation& d : deltas)
        {
            if (nonMonotonicBuckets[d.referent_base_index]
                == std::numeric_limits<std::size_t>::max())
            {
                nonMonotonicBuckets[d.referent_base_index] = nextBucket++;
            }

            nonMonotonicBuckets[d.referent_base_index + 1] =
                nonMonotonicBuckets[d.referent_base_index];
        }

        for (std::size_t& b : nonMonotonicBuckets)
        {
            if (b == ~0UZ)
            {
                b = nextBucket++;
            }
        }

        // {
        //     std::string msg {};

        //     for (auto& b : nonMonotonicBuckets)
        //     {
        //         msg += std::format("{{b{}}}, ", b);
        //     }

        //     util::logTrace("post nonMonotomicBuckets {}", msg);
        // }

        std::vector<InclusiveRange> output {};
        output.resize(
            nextBucket,
            InclusiveRange {.start {std::numeric_limits<std::size_t>::max()}, .end {0UZ}});

        for (std::size_t i = 0; i < mergedSortedRanges.size(); ++i)
        {
            const std::size_t bucket = nonMonotonicBuckets[i];

            output[bucket].start = std::min({output[bucket].start, mergedSortedRanges[i].start});
            output[bucket].end   = std::max({output[bucket].end, mergedSortedRanges[i].end});
        }

        // util::logTrace("Output: {}", formatVectorRanges(output));

        return output;
    }
} // namespace util