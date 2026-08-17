#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace BidirectionalInMemGraph
{
    using EdgeTableRange = RangeOfAPC;

    EdgeTableRange EdgeTableConstructor::ReadAnEdgeTableRange_(
        FabricSegments edge_table,
        uint32_t edge_idx
    ) noexcept
    {
        EdgeTableRange range{};
        
        uint64_t& desired_begin = edge_table == FabricSegments::HORIZONTAL_EDGE_TABLE ? 
            HorizontalEdgeBeginIdx_ : VerticalEdgeBeginIdx_;

        range.BeginIndex = desired_begin + static_cast<uint64_t>(edge_idx) * EdgeBuilder::EDGE_TABLE_RECORD_WIDTH;
        range.EndIndex = range.BeginIndex + EdgeBuilder::EDGE_TABLE_RECORD_WIDTH;
        range.IsValid = 
            CoreOfFabricCoordinator::IsValidEdgeTable(edge_table) &&
            edge_idx < CountOfAPC_ &&
            desired_begin > DescriptionBeginIdx_ &&
            range.BeginIndex >= desired_begin &&
            range.EndIndex < SlabCellCount_;
        return range;
    }

    bool EdgeTableConstructor::ReadEdgedataAtomically(
        FabricSegments edge_table,
        uint32_t edge_idx,
        EdgeBuilder::EdgeLockValues& values
    ) noexcept
    {
        uint64_t raw_st_lock = FABRIC_CELL_SENTINAL;
        EdgeTableRange range = ReadAnEdgeTableRange_(edge_table, edge_idx);

        if (
            !range.IsValid ||
            !AtomicallyLoadReadAUnit(
                range.BeginIndex + static_cast<uint8_t>(EdgeBuilder::EdgeTableIndexing::SEQLOCK_STATE),
                raw_st_lock
            ) 
        )
        {
            return false;
        }
        return DSA::GetSeqLockAndLifeCycle(raw_st_lock, values);
    }

    void EdgeTableConstructor::InitializeEdgeTable_(FabricSegments edge_table) noexcept
    {
        if (!CoreOfFabricCoordinator::IsValidEdgeTable(edge_table))
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
        EdgeBuilder::EdgeBuffer& return_buffer,
        uint32_t max_tries
    ) noexcept
    {
        const EdgeTableRange range = ReadAnEdgeTableRange_(edge_table, edge_idx);

        if (!range.IsValid)
        {
            return false;
        }
        const uint8_t internal_idx_st = static_cast<uint8_t>(EdgeBuilder::EdgeTableIndexing::SEQLOCK_STATE);
        const size_t control_idx = range.BeginIndex + internal_idx_st;
        uint64_t before_read = FABRIC_CELL_SENTINAL;

        for (size_t i = 0; i < max_tries; i++)
        {

            if (
                !AtomicallyLoadReadAUnit(control_idx, before_read)
            )
            {
                return false;
            }

            if (
                !ReadBufferwithSyncAtomicIndex(
                    range.BeginIndex,
                    EdgeBuilder::EDGE_TABLE_RECORD_WIDTH,
                    return_buffer.data(),
                    static_cast<uint8_t>(EdgeBuilder::EdgeTableIndexing::SEQLOCK_STATE)
                )
            )
            {
                return false;
            }
            
            if (
                !EdgeBuilder::ValidateEdgeBuffer(edge_table, return_buffer) ||
                before_read != return_buffer[internal_idx_st]
            )
            {
                continue;
            }

            return true;
        }
        return false;
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
            if (                
                CompareExchangeStrongFromFabric(
                    control_idx,
                    expected_st_lock,
                    buffer[static_cast<uint8_t>(EdgeBuilder::EdgeTableIndexing::SEQLOCK_STATE)]
                )
            )
            {
                return true;
            }
        }
        
        return false;
    }

    bool EdgeTableConstructor::PublishReservedEdge_(
        EdgeBuilder::EdgeData& desired_data,
        uint32_t edge_idx
    ) noexcept
    {
        EdgeBuilder::EdgeBuffer buffer{};
        const EdgeTableRange range = ReadAnEdgeTableRange_(desired_data.EdgeTable, edge_idx);

        EdgeBuilder::EdgeLockValues edge_status{};
        if (
            !ReadEdgedataAtomically(desired_data.EdgeTable, edge_idx, edge_status) ||
            edge_status.StateOfTheAPC != EdgeBuilder::EdgeStatus::RESERVED
        )
        {
            return false;
        }

        desired_data.SeqLock = edge_status.SeqLock + 1u;

        return 
            EdgeBuilder::BuildEdgeBuffer(buffer, desired_data) &&
            EdgeBuilder::IsTransitionStateLeagal(edge_status.StateOfTheAPC, desired_data.Status) &&
            ForceNxLenMemCopy(
                range.BeginIndex,
                EdgeBuilder::EDGE_TABLE_RECORD_WIDTH,
                buffer.data()
            );
    }

}