#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace BidirectionalInMemGraph
{
    using EdgeTableRange = DescriptorConf::APCDescriptorRange;

    EdgeTableRange EdgeTableConstructor::ReadAnEdgeTableRange(
        FabricSegments edge_table,
        uint32_t edge_idx
    ) noexcept
    {
        EdgeTableRange range{};
        RBC::RecordBookTablesBoundsCarrier edge_table_bounds{};

        if (
            !DSA::IsValidEdgeTable(edge_table) ||
            edge_idx >= CountOfAPC_ ||
            !GetRecordMapCarrierRanges_(
                edge_table,
                edge_table_bounds
            )
        )
        {
            return range;
        }

        range.BeginIndex = edge_table_bounds.BeginIndex + static_cast<uint64_t>(edge_idx) * EdgeBuilder::EDGE_TABLE_RECORD_WIDTH;
        range.EndIndex = range.BeginIndex + EdgeBuilder::EDGE_TABLE_RECORD_WIDTH;
        range.IsValid = true;

        if (
            range.BeginIndex < edge_table_bounds.BeginIndex ||
            range.BeginIndex >= edge_table_bounds.EndIndex ||
            range.EndIndex > edge_table_bounds.EndIndex ||
            range.EndIndex > SlabCellCount_
        )
        {
            range.IsValid = false;
        }
        return range;
    }


    void InitializeEdgeTable_(FabricSegments edge_table) noexcept;


}