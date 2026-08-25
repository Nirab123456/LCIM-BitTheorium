#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"

namespace BidirectionalInMemGraph
{
    bool FabricToAPCLinker::AtomicallyReadLongLongAPCUnit(
        uint64_t idx,
        uint64_t& return_value
    ) noexcept
    {
        return 
            IsThisAPCValid() &&
            FabricOwnerPtr_->AtomicallyLoadReadAUnit(
                RangeOfThisAPCInSlab_.BeginIndex + idx,
                return_value
            );
    }

    void FabricToAPCLinker::AtomicallyWriteU64ToAPC(
        uint64_t idx,
        const uint64_t& value
    ) noexcept
    {
        if (
            !IsThisAPCValid()
        )
        {
            return;
        }
        FabricOwnerPtr_->AtomicallyStoreU64Fab(
            idx + RangeOfThisAPCInSlab_.BeginIndex,
            value
        );
    }

    bool FabricToAPCLinker::BindExternalRawFabricBacking_(
        uint64_t* words_raw,
        uint32_t cell_count,
        VagueTemoraryPremativeFabric* fabric_owner,
        uint64_t fabric_slot_idx
    ) noexcept
    {
        if (
            !words_raw ||
            !fabric_owner ||
            !APCDataStructure::IsCapacityOfAPCValid(cell_count) ||
            !APCDataStructure::IsValid32BitAPCUnit(fabric_slot_idx)
        )
        {
            return false;
        }
        const RangeOfAPC range_of_this_apc = fabric_owner->GetSegmentPoolRange(fabric_slot_idx);
        if (
            !range_of_this_apc.IsValid ||
            range_of_this_apc.EndIndex - range_of_this_apc.BeginIndex != cell_count
        )
        {
            return false;
        }
        APCSlotIdx_ = static_cast<uint32_t>(fabric_slot_idx);
        RawAPCBasePtr_ = reinterpret_cast<std::byte*>(words_raw);
        CapacityOfThisAPC_ = cell_count;
        FabricOwnerPtr_ = fabric_owner;
        RangeOfThisAPCInSlab_ = range_of_this_apc;
        return true;
    }


    void FabricToAPCLinker::ReleseFabricBindingOnly_() noexcept
    {
        RangeOfThisAPCInSlab_ = RangeOfAPC{};
        CapacityOfThisAPC_ = UNSIGNED_ZERO;
        FabricOwnerPtr_ = nullptr;
        RawAPCBasePtr_ = nullptr;
        APCSlotIdx_ = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
    }

    bool FabricToAPCLinker::ForceCopyToAPCFromBuffer(
        uint32_t starting_idx_in_apc,
        uint32_t sequential_number_of_cells,
        const uint64_t* source_cells
    ) noexcept
    {
        return 
            IsThisAPCValid() &&
            starting_idx_in_apc + sequential_number_of_cells <= CapacityOfThisAPC_ &&
            FabricOwnerPtr_->ForceNxLenMemCopy(
                (RangeOfThisAPCInSlab_.BeginIndex + starting_idx_in_apc), 
                sequential_number_of_cells, 
                source_cells
            );
    }

    bool FabricToAPCLinker::CopyFromAPCToBuffer(
        uint32_t starting_idx_in_apc,
        uint32_t sequential_number_of_cells,
        uint64_t* return_buffer
    ) noexcept
    {   
        return 
            IsThisAPCValid() &&
            FabricOwnerPtr_->ReadASnapShotFromSlab(
                (RangeOfThisAPCInSlab_.BeginIndex + starting_idx_in_apc), 
                sequential_number_of_cells, 
                return_buffer
            );
    }

    bool FabricToAPCLinker::InitiateAPCMetaHeader(
        const LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier& layout_weight,
        const SchemaDefinition::InitialRegionalDtypeConf& dtype_conf,
        const SchemaDefinition::InitialRegionalProtocol& protocol_conf,
        uint8_t version
    ) noexcept
    {
        using IAB = InstallAxisToBuffer;
        using DSA = DescriptionOfAPC;

        HeaderOrchestrator::APCMetaBuffer header_meta_buffer{};
        IAB::BufferOfAPCIdentity idintity_buffer{};

        const SeqLockedOperation read_identity_buffer_ok = FabricOwnerPtr_->ReadIdentityBufferOfAPC(static_cast<uint32_t>(APCSlotIdx_), idintity_buffer);
        DSA::SeqLockAndStateStruct current_state = FabricOwnerPtr_->ReadAPCStateAtomically_(APCSlotIdx_);

        if (
            !IsThisAPCValid() ||
            read_identity_buffer_ok != SeqLockedOperation::FOUND ||
            !current_state.IsValid ||
            current_state.StateOfTheAPC != StateOfAPC::RESERVED ||
            !HeaderOrchestrator::InitializeDefaultHeaderBuffer(
            header_meta_buffer,
            idintity_buffer,
            CapacityOfThisAPC_,
            layout_weight,
            dtype_conf,
            protocol_conf,
            version
            ) ||
            !HeaderOrchestrator::IsHeaderBufferValidationMarked(header_meta_buffer)
        )
        {
            return false;
        }

        current_state.StateOfTheAPC = StateOfAPC::LIVE;
        ++current_state.SeqLock;
        const uint64_t raw_new_state_seq = DSA::ComposeSeqLockAndState(current_state);
        header_meta_buffer[static_cast<uint8_t>(HeaderIdentifierOfAPC::APC_LIFE_CYCLE)] = raw_new_state_seq;

        return 
            APCDataStructure::IsValidFabricUnit(raw_new_state_seq) &&
            ForceCopyToAPCFromBuffer(
                UNSIGNED_ZERO,
                APCDataStructure::METACELL_COUNT,
                header_meta_buffer.data()
            );
    }

    bool FabricToAPCLinker::ReadAPCMetaUnit(
        HeaderIdentifierOfAPC meta_idx,
        uint64_t& return_value
    ) noexcept
    {
        if (!IsThisAPCValid())
        {
            return false;
        }
        const uint8_t idx_u = static_cast<uint8_t>(meta_idx);
        const size_t slab_idx = static_cast<uint64_t>(RangeOfThisAPCInSlab_.BeginIndex + idx_u);
        return FabricOwnerPtr_->AtomicallyLoadReadAUnit(slab_idx, return_value);
    }


}