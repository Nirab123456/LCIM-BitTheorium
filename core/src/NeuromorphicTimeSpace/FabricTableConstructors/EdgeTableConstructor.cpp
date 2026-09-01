#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace BidirectionalInMemGraph
{
    using EdgeTableRange = RangeOfAPC;

    std::span<EdgeTableConf::ParentRelation> EdgeTableConstructor::ParentRelation_(
        FabricSegments edge_table,
        uint32_t edge_idx
    ) noexcept
    {
        const EdgeTableRange range = ReadAnEdgeTableRange_(edge_table, edge_idx);
        if (!range.IsValid)
        {
            return {};
        }

        auto* first = std::launder(reinterpret_cast<EdgeBuilder::ParentRelation*>(
            SlabBasePtr_ + range.BeginIndex + 1u
        ));

        return {
            first,
            static_cast<size_t>(MaxDirectParentsPerAxis_)
        };
    }

    bool EdgeTableConstructor::ConstructParentRelationObject_(
        FabricSegments edge_table,
        uint32_t edge_idx
    ) noexcept
    {
        const EdgeTableRange range = ReadAnEdgeTableRange_(edge_table, edge_idx);
        if (!range.IsValid)
        {
            return false;
        }

        auto* first = reinterpret_cast<EdgeBuilder::ParentRelation*>(SlabBasePtr_ + range.BeginIndex + 1u);

        for (uint8_t i = 0; i < MaxDirectParentsPerAxis_; i++)
        {
            std::construct_at(first + i);
        }
        
        return true;
    }

    EdgeTableRange EdgeTableConstructor::ReadAnEdgeTableRange_(
        FabricSegments edge_table,
        uint32_t edge_idx
    ) noexcept
    {
        EdgeTableRange range{};
        if (
            !CoreOfFabricCoordinator::IsValidEdgeTable(edge_table) ||
            edge_idx >= CountOfAPC_ ||
            EdgeTableRecordWidth_ != EdgeBuilder::EdgeTableRecordWidth(MaxDirectParentsPerAxis_)
        )
        {
            return range;
        }
        
        const uint64_t range_begin = edge_table == FabricSegments::HORIZONTAL_EDGE_TABLE ?
            HorizontalEdgeBeginIdx_ : VerticalEdgeBeginIdx_;

        range.BeginIndex = range_begin + static_cast<uint64_t>(edge_idx) * EdgeTableRecordWidth_;
        range.EndIndex = range.BeginIndex + EdgeTableRecordWidth_;
        range.IsValid = range.BeginIndex >= range_begin &&
            range.EndIndex <= SlabCellCount_;

        return range;
    }

}