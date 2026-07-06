#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"

namespace PredictedAdaptedEncoding
{

    bool ReadAndWriteOfAPC::ReadCompleateMetaHeaderDirectlyNonAtomic(HeaderOrchestrator::APCMetaBuffer& a_default_buffer) noexcept
    {
        if (!RangeOfThisAPCInSlab_.IsValid)
        {
            return false;
        }

        if (!CopyFromAPCToBuffer(UNSIGNED_ZERO, APCDataStructure::METACELL_COUNT, a_default_buffer.data()))
        {
            return false;
        }
        
        return true;
    }

    bool ReadAndWriteOfAPC::ReadCompleatLayoutBuffer(
        LayoutBoundsOrchestrator::TrackingBufferOfAPC& a_layout_buffer,
        bool is_claimed_required
    ) noexcept
    {
        if (is_claimed_required)
        {
            return ClaimAndCopyToAPCFromBuffer(
                PageNodeOrchestrator::LayoutBufferBegainInMetaIndecies(),
                PageNodeOrchestrator::TrackedAPCNodeLen(),
                a_layout_buffer.data()
            );
        }
        
        return ForceCopyToAPCFromBuffer(
            PageNodeOrchestrator::LayoutBufferBegainInMetaIndecies(),
            PageNodeOrchestrator::TrackedAPCNodeLen(),
            a_layout_buffer.data()
        );
    }

    bool ReadAndWriteOfAPC::InitiateAPCMetaHeader(
        uint16_t total_capacity,
        APCGroupReserver::APCInitialIdentityStruct& container_configuration,
        const LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier& user_defined_weight,
        uint8_t version,
        LocalityPolicy locality
    ) noexcept
    {
        HeaderOrchestrator::APCMetaBuffer header_meta_buffer{};

        if (!HeaderOrchestrator::InitializeDefaultHeaderBuffer(
            header_meta_buffer,
            container_configuration,
            total_capacity,
            user_defined_weight,
            version,
            locality
        ))
        {
            return false;
        }
        
        if (!HeaderOrchestrator::IsHeaderBufferValidationMarked(header_meta_buffer))
        {
            return false;
        }

        return ForceCopyToAPCFromBuffer(
            UNSIGNED_ZERO,
            APCDataStructure::METACELL_COUNT,
            header_meta_buffer.data()
        );
        
    }


    uint64_t ReadAndWriteOfAPC::AtomicallyUpdateACounter(uint16_t desired_idx, uint32_t delta) noexcept
    {
        if (
            !RangeOfThisAPCInSlab_.IsValid ||
            !FabricOwnerPtr_ ||
            desired_idx >= CapacityOfThisAPC_
        )
        {
            return PackedCell64_t::PACKED_CELL_SENTINAL;
        }
        const uint64_t desired_slab_idx = RangeOfThisAPCInSlab_.BeginIndex + desired_idx;
        packed64_t current_cell = FabricOwnerPtr_->AtomicallyLoadReadCompletePackedCell(desired_slab_idx);

        uint64_t updated_count = UNSIGNED_ZERO;

        const packed64_t updated_cell = Mutation64_t::AddDeltaInAtomicUnsignedCell(delta, current_cell, &updated_count);

        if (updated_cell == PackedCell64_t::PACKED_CELL_SENTINAL)
        {
            return PackedCell64_t::PACKED_CELL_SENTINAL;
        }

        FabricOwnerPtr_->AtomicallyStorePackedCellUnchecked(desired_slab_idx, updated_cell, MoStoreSeq_);
        return updated_count;
    }

    uint64_t ReadAndWriteOfAPC::AtomicallyUpdateMetaCellCounter(MetaIndexOfAPCNode meta_idx, uint32_t delta) noexcept
    {
        const uint16_t desired_idx = static_cast<uint16_t>(meta_idx);
        if (desired_idx < APCDataStructure::METACELL_COUNT)
        {
            return AtomicallyUpdateACounter(desired_idx, delta);
        }
        return PackedCell64_t::PACKED_CELL_SENTINAL;
    }
}