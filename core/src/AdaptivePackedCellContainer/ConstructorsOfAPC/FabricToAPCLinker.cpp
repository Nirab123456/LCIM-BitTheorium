#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"

namespace BidirectionalInMemGraph
{
    bool FabricToAPCLinker::BindExternalRawFabricBacking_(
        uint64_t* raw_cells_ptr,
        VagueTemoraryPremativeFabric* fabric_owner,
        uint64_t fabric_slot_idx,
        uint64_t* generation_cell,
        uint32_t expected_generation
    ) noexcept
    {
        if (
            !raw_cells_ptr ||
            !fabric_owner ||
            !APCDataStructure::IsCapacityOfAPCValid(fabric_owner->PerAPCRuntimeCellCount_) ||
            !APCDataStructure::IsValid32BitAPCUnit(fabric_slot_idx) ||
            IsFabricBound_()
        )
        {
            return false;
        }
        const RangeOfAPC range_of_this_apc = fabric_owner->GetSegmentPoolRange(fabric_slot_idx);
        if (
            !range_of_this_apc.IsValid ||
            range_of_this_apc.EndIndex - range_of_this_apc.BeginIndex != fabric_owner->PerAPCRuntimeCellCount_
        )
        {
            return false;
        }
        APCSlotIdx_ = static_cast<uint32_t>(fabric_slot_idx);
        RawAPCBasePtr_ = reinterpret_cast<std::byte*>(raw_cells_ptr);
        CapacityOfThisAPC_ = fabric_owner->PerAPCRuntimeCellCount_;
        FabricOwnerPtr_ = fabric_owner;
        RangeOfThisAPCInSlab_ = range_of_this_apc;
        APCGenerationCellPtr_ = generation_cell;
        ExpectedGeneration_ = expected_generation;
        return true;
    }


    void FabricToAPCLinker::ReleseFabricBindingOnly_() noexcept
    {
        RangeOfThisAPCInSlab_ = RangeOfAPC{};
        CapacityOfThisAPC_ = UNSIGNED_ZERO;
        FabricOwnerPtr_ = nullptr;
        RawAPCBasePtr_ = nullptr;
        APCSlotIdx_ = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        APCGenerationCellPtr_ = nullptr;
        ExpectedGeneration_ = UNSIGNED_ZERO;
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
            FabricOwnerPtr_->ForceNxLenMemCopy(
                (RangeOfThisAPCInSlab_.BeginIndex + UNSIGNED_ZERO), 
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

    bool FabricToAPCLinker::IsFabricBound_() const noexcept
    {
        return FabricOwnerPtr_ != nullptr &&
            RangeOfThisAPCInSlab_.IsValid &&
            RawAPCBasePtr_ != nullptr &&
            APCGenerationCellPtr_ != nullptr &&
            APCDataStructure::IsValid32BitAPCUnit(APCSlotIdx_) &&
            HandleOfAPCStatic::IsGenerationValid(ExpectedGeneration_);
    }


    APCUseScope FabricToAPCLinker::AcquireAPCUse_() noexcept
    {
        if (!IsFabricBound_())
        {
            return APCUseScope{};
        }

        std::atomic_ref<uint64_t> control(*APCGenerationCellPtr_);

        const uint64_t before = control.fetch_add(1u, std::memory_order_acquire);

        const HandleOfAPCStatic::ControlValues values = HandleOfAPCStatic::ReadControlCell(before);

        if (
            values.Closed ||
            values.Generation != ExpectedGeneration_ ||
            values.ActiveAccess == APCDataStructure::APC_INDEX_BOUND_SENTINAL
        )
        {
            control.fetch_sub(1u, std::memory_order_release);
            return APCUseScope{};
        }
        
        return APCUseScope(APCGenerationCellPtr_);
    }

}