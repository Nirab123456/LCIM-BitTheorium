#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"


namespace PredictedAdaptedEncoding
{
    bool APCSegmentsCausalCordinator::PublishCausal(
        AdaptivePackedCellContainer& apc,
        APCPagedNodeSegmentClasses region,
        packed64_t cell,
        std::atomic<uint64_t>* growth_counter
    ) noexcept
    {
        (void) growth_counter;

        cell = PackedCell64_t::SetPageClassInPacked(cell, region);

        if (apc.TryPublishRegionalSharedGrowthOnce(region, cell))
        {
            MarkEmittedCausalCell(region, cell);
            return true;
        }
        return false;
    }

    std::optional<packed64_t> APCSegmentsCausalCordinator::ConsumeCausal(
        AdaptivePackedCellContainer& apc,
        APCPagedNodeSegmentClasses region,
        size_t& scan_cursor,
        std::atomic<uint64_t>* older_counter,
        bool drop_older
    ) noexcept
    {
        const size_t max_drops = drop_older
            ? (static_cast<size_t>(PayloadCapacityFromHeader()) * 2u + 16u)
            : 1u;
        size_t drops = 0;

        while (drops < max_drops)
        {
            auto maybe = apc.ConsumeCellByRegionMaskTraverseStartFromThisAPC(region, scan_cursor);
            if (!maybe) return std::nullopt;

            if (AcceptCausalCell(region, *maybe))
                return maybe;

            if (older_counter)
                older_counter->fetch_add(1, std::memory_order_relaxed);

            if (!drop_older)
                return maybe;

            ++drops;
        }
        return std::nullopt;  // drop budget exhausted — caller should back off
    }

}