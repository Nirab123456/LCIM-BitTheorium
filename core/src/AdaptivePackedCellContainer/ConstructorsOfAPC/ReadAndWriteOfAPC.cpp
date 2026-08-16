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
        HeaderOrchestrator::APCMetaBuffer header_meta_buffer{};
        IAB::BufferOfAPCIdentity idintity_buffer{};

        const std::optional<StateOfAPC> current_state = FabricOwnerPtr_->ReadIdentityBufferOfAPC(static_cast<uint32_t>(APCSlotIdx_), idintity_buffer);

        if (
            !IsThisAPCValid() ||
            !current_state.has_value() ||
            current_state != StateOfAPC::RESERVED ||
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
        return ForceCopyToAPCFromBuffer(
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
        if (!IsValidAPCRange(idx_u, 1u))
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

        return IsValidAPCRange(local_idx_u, 1u) &&
            FabricOwnerPtr_->CompareExchangeStrongFromFabric(
                RangeOfThisAPCInSlab_.BeginIndex + local_idx_u,
                expected_value,
                desired_value
            );
    }


}