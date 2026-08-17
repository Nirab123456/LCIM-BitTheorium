#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"

namespace BidirectionalInMemGraph
{

    bool ReadAndWriteOfAPC::ReadCompleatLayoutBuffer_(
        LayoutBoundsOrchestrator::TrackingBufferOfAPC& layout_buffer
    ) noexcept
    {
        BufferConfForTracking::BuildNullTrackingBuffer(layout_buffer);
        if (!CopyFromAPCToBuffer(
            APCDataStructure::LayoutBufferBegainInMetaIndecies(),
            APCDataStructure::CountOfMacroColumn(),
            layout_buffer.data()
        ))
        {
            return false;
        }
        
        return LayoutBoundsOrchestrator::ValidateALayoutBuffer(layout_buffer, CapacityOfThisAPC_);
    }

    bool ReadAndWriteOfAPC::InitiateAPCMetaHeader(
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

        const bool read_identity_buffer_ok = FabricOwnerPtr_->ReadIdentityBufferOfAPC(static_cast<uint32_t>(APCSlotIdx_), idintity_buffer);
        DSA::SeqLockAndStateStruct current_state = FabricOwnerPtr_->ReadAPCStateAtomically_(APCSlotIdx_);

        if (
            !IsThisAPCValid() ||
            !read_identity_buffer_ok ||
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

    bool ReadAndWriteOfAPC::ReadAPCMetaUnit(
        HeaderIdentifierOfAPC meta_idx,
        uint64_t& return_value,
        bool atomic_required
    ) noexcept
    {
        const uint8_t idx_u = static_cast<uint8_t>(meta_idx);
        if (!IsThisAPCValid())
        {
            return false;
        }
        
        const size_t slab_idx = static_cast<uint64_t>(RangeOfThisAPCInSlab_.BeginIndex + idx_u);
        uint64_t meta_value = UNSIGNED_ZERO;
        bool read_ok =  atomic_required ? 
            FabricOwnerPtr_->AtomicallyLoadReadAUnit(slab_idx, meta_value) :
            FabricOwnerPtr_->ReadAFabricU64Directly(slab_idx, meta_value);

        if (!read_ok)
        {
            return false;
        }
        return_value = meta_value;
        return true;
    }

    bool ReadAndWriteOfAPC::CompareExchangeAPCMetaUinit(
        HeaderIdentifierOfAPC meta_idx,
        uint64_t& expected_value,
        uint64_t desired_value
    ) noexcept
    {
        const uint8_t local_idx_u = static_cast<uint8_t>(meta_idx);

        return 
            IsThisAPCValid() &&
            FabricOwnerPtr_->CompareExchangeStrongFromFabric(
                    RangeOfThisAPCInSlab_.BeginIndex + local_idx_u,
                    expected_value,
                    desired_value
                );
    }


}