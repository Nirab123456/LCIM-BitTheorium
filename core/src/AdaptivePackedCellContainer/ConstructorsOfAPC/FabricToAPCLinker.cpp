#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"

namespace BidirectionalInMemGraph
{
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

    uint64_t* APCHandleAndRetirement::GetAPCGenerationPtr_(uint32_t slot) noexcept
    {
        if (
            !SlabBasePtr_ ||
            slot >= CountOfAPC_ ||
            HandleTableBeginIndex_ >= SlabCellCount_
        )
        {
            return nullptr;
        }
        
        const size_t idx = HandleTableBeginIndex_ + HandleOfAPCStatic::CellOffset(slot);

        return idx < SlabCellCount_ ? &SlabBasePtr_[idx] : nullptr;
    }

    bool APCHandleAndRetirement::InitializeAPCGenerationTable_() noexcept
    {
        for (uint32_t slot = 0; slot < CountOfAPC_; slot++)
        {
            uint64_t* cell = GetAPCGenerationPtr_(slot);
            if (!cell)
            {
                return false;
            }

            HandleOfAPCStatic::ControlValues values{};
            values.Generation = HandleOfAPCStatic::FIRST_GENERATION;
            values.ActiveAccess = UNSIGNED_ZERO;
            values.Closed = true;


            std::atomic_ref<uint64_t>(*cell).store(
                HandleOfAPCStatic::MakeControlCell(values),
                std::memory_order_relaxed
            );
        }
        return true;
    }

    bool APCHandleAndRetirement::OpenAPCGeneration_(uint32_t slot, uint32_t generation) noexcept
    {
        uint64_t* cell = GetAPCGenerationPtr_(slot);

        if (!cell || !HandleOfAPCStatic::IsGenerationValid(generation))
        {
            return false;
        }

        HandleOfAPCStatic::ControlValues values{};
        values.Generation = generation;
        values.ActiveAccess = UNSIGNED_ZERO;
        values.Closed = false;
        
        uint64_t expected = HandleOfAPCStatic::MakeControlCell(values);
        //desired
        values.Closed = true;

        return std::atomic_ref<uint64_t>(*cell).compare_exchange_strong(
            expected,
            HandleOfAPCStatic::MakeControlCell(values),
            std::memory_order_acq_rel,
            std::memory_order_acquire
        );
    }

    bool APCHandleAndRetirement::AdvanceClosedAPCGeneration_(uint32_t slot, uint32_t& generation_new) noexcept
    {
        generation_new = UNSIGNED_ZERO;
        uint64_t* cell = GetAPCGenerationPtr_(slot);

        if (!cell)
        {
            return false;
        }

        std::atomic_ref<uint64_t> control(*cell);
        uint64_t observed = control.load(std::memory_order_acquire);

        const HandleOfAPCStatic::ControlValues values = HandleOfAPCStatic::ReadControlCell(observed);

        HandleOfAPCStatic::ControlValues desired_values{};


        const uint32_t desired_generation = HandleOfAPCStatic::NextGeneration(values.Generation);

        desired_values.Generation = desired_generation;
        desired_values.ActiveAccess = UNSIGNED_ZERO;
        desired_values.Closed = true;

        const uint64_t desired = HandleOfAPCStatic::MakeControlCell(desired_values);
        
        if (
            !values.Closed ||
            values.ActiveAccess != UNSIGNED_ZERO ||
            desired_generation == UNSIGNED_ZERO 
        )
        {
            return false;
        }
        
        if (
            !control.compare_exchange_strong(observed, desired, std::memory_order_acq_rel, std::memory_order_acquire)
        )
        {
            return false;
        }
        
        generation_new = desired_generation;
        return true;
    }

}