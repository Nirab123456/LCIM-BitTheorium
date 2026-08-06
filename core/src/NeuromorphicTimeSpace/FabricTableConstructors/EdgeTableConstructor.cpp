#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace BidirectionalInMemGraph
{
    using EdgeTableRange = DescriptorConf::APCDescriptorRange;

    EdgeTableRange EdgeTableConstructor::ReadAnEdgeTableRange_(
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


    void EdgeTableConstructor::InitializeEdgeTable_(FabricSegments edge_table) noexcept
    {
        if (!EdgeBuilder::IsValidEdgeTable(edge_table))
        {
            return;
        }

        EdgeBuilder::EdgeData free_edge{};
        EdgeBuilder::EdgeBuffer buffer{};

        for (uint32_t i = 0; i < CountOfAPC_; i++)
        {
            const EdgeTableRange range_edge_i = ReadAnEdgeTableRange_(edge_table, i);
            if (!range_edge_i.IsValid)
            {
                return;
            }
            if (
                !EdgeBuilder::BuildFreeEdgeTable(
                    edge_table,
                    i,
                    free_edge
                ) ||
                !EdgeBuilder::BuildEdgeBuffer(
                    buffer,
                    free_edge
                )
            )
            {
                return;
            }
            ForceNxLenMemCopy(
                range_edge_i.BeginIndex,
                EdgeBuilder::EDGE_TABLE_RECORD_WIDTH,
                buffer.data()
            );
        }
    }

    bool EdgeTableConstructor::ReadAnEdgeBuffer_(
        FabricSegments edge_table,
        uint32_t edge_idx,
        EdgeBuilder::EdgeBuffer& return_buffer
    ) noexcept
    {
        const EdgeTableRange range = ReadAnEdgeTableRange_(edge_table, edge_idx);

        if (!range.IsValid)
        {
            return false;
        }
        const size_t control_idx = range.BeginIndex + static_cast<uint8_t>(EdgeBuilder::EdgeTableIndexing::SEQLOCK_STATE);
        return 
            ReadASnapShotFromSlab(
                range.BeginIndex,
                EdgeBuilder::EDGE_TABLE_RECORD_WIDTH,
                return_buffer.data()
            );
    }

    EdgeBuilder::EdgeStatus EdgeTableConstructor::ReadEdgeData_(
        FabricSegments edge_table,
        uint32_t edge_idx,
        EdgeBuilder::EdgeData& edge_data,
        EdgeBuilder::EdgeBuffer* edge_buffer_return
    ) noexcept
    {
        EdgeBuilder::EdgeBuffer buffer{};
        if (
            !ReadAnEdgeBuffer_(edge_table, edge_idx, buffer)
        )
        {
            return EdgeBuilder::EdgeStatus::INVALID;
        }

        if (edge_buffer_return)
        {
            *edge_buffer_return = buffer;
        }

        return 
            EdgeBuilder::ReadEdgeFromBufferStatically(
                edge_table,
                buffer,
                edge_data
            );
    }


}