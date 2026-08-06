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

    std::optional<EdgeBuilder::EdgeStatus> EdgeTableConstructor::ReadEdgeData_(
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
            return std::nullopt;
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

    bool EdgeTableConstructor::SwitchEdgeState__(
        FabricSegments edge_table,
        uint32_t edge_idx,
        EdgeBuilder::EdgeData& pre_switch,
        EdgeBuilder::EdgeStatus desired_state,
        std::optional<EdgeBuilder::EdgeStatus> required_st,
        uint32_t max_tries
    ) noexcept
    {
        EdgeBuilder::EdgeBuffer buffer{};
        const EdgeTableRange range = ReadAnEdgeTableRange_(edge_table, edge_idx);
        const size_t control_idx = range.BeginIndex + static_cast<uint8_t>(EdgeBuilder::EdgeTableIndexing::SEQLOCK_STATE);

        
        for (size_t i = 0; i < max_tries; i++)
        {
            std::optional<EdgeBuilder::EdgeStatus> edge_status_current = ReadEdgeData_(
                edge_table,
                edge_idx,
                pre_switch,
                &buffer
            );

            if (
                !edge_status_current.has_value() ||
                (
                    edge_status_current.value() == EdgeBuilder::EdgeStatus::HAULTED &&
                    desired_state != EdgeBuilder::EdgeStatus::LIVE
                )
            )
            {
                return false;
            }

            if (
                required_st.has_value() &&
                required_st != edge_status_current
            )
            {
                continue;
            }
            
            const bool caller_holds_reservation = edge_status_current == EdgeBuilder::EdgeStatus::RESERVED;
            const bool false_owner_claim = !caller_holds_reservation && desired_state != EdgeBuilder::EdgeStatus::RESERVED;
            const bool non_ower_touching_reserved = caller_holds_reservation && desired_state == EdgeBuilder::EdgeStatus::RESERVED;

            if ( 
                false_owner_claim ||
                non_ower_touching_reserved ||
                !EdgeBuilder::IsTransitionStateLeagal(edge_status_current.value(), desired_state)
            )
            {
                continue;
            }

            uint64_t expected_st_lock = buffer[static_cast<uint8_t>(EdgeBuilder::EdgeTableIndexing::SEQLOCK_STATE)];

            ++pre_switch.SeqLock;
            pre_switch.Status = desired_state;

            if (
                !EdgeBuilder::BuildEdgeBuffer(buffer, pre_switch)
            )
            {
                return false;
            }

            --pre_switch.SeqLock;
            pre_switch.Status = edge_status_current.value();

            return
                CompareExchangeStrongFromFabric(
                    control_idx,
                    expected_st_lock,
                    buffer[static_cast<uint8_t>(EdgeBuilder::EdgeTableIndexing::SEQLOCK_STATE)]
                );
        }
        
    }

    bool EdgeTableConstructor::PublishReservedEdge_(
        EdgeBuilder::EdgeData& desired_data,
        uint32_t edge_idx
    ) noexcept
    {
        EdgeBuilder::EdgeData edge_data_current{};
        EdgeBuilder::EdgeBuffer buffer{};
        const EdgeTableRange range = ReadAnEdgeTableRange_(desired_data.EdgeTable, edge_idx);
        std::optional<EdgeBuilder::EdgeStatus> current_status = ReadEdgeData_(
            desired_data.EdgeTable,
            edge_idx,
            edge_data_current
        );

        ++desired_data.SeqLock;

        if (
            !range.IsValid ||
            !current_status.has_value() ||
            current_status.value() != EdgeBuilder::EdgeStatus::RESERVED ||
            !EdgeBuilder::BuildEdgeBuffer(buffer, desired_data)
        )
        {
            return false;
        }

        return 
            AtomicallyCopyFromBufferToFabric(
                range.BeginIndex,
                EdgeBuilder::EDGE_TABLE_RECORD_WIDTH,
                buffer.data()
            );
    }



}