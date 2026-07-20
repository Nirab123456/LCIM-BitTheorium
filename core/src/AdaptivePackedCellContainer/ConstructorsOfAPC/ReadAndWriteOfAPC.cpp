#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"

namespace PredictedAdaptedEncoding
{

    bool ReadAndWriteOfAPC::ReadCompleateMetaHeaderDirectlyNonAtomic_(HeaderOrchestrator::APCMetaBuffer& a_default_buffer) noexcept
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

    bool ReadAndWriteOfAPC::ReadCompleatLayoutBuffer_(
        LayoutBoundsOrchestrator::TrackingBufferOfAPC& layout_buffer,
        bool atomic_required 
    ) noexcept
    {
        BufferConfForTracking::BuildNullTrackingBuffer(layout_buffer);
        if (!CopyFromAPCToBuffer(
            APCDataStructure::LayoutBufferBegainInMetaIndecies(),
            APCDataStructure::CountOfMacroColumn(),
            layout_buffer.data(),
            atomic_required
        ))
        {
            return false;
        }
        
        return LayoutBoundsOrchestrator::ValidateALayoutBuffer(layout_buffer, CapacityOfThisAPC_);
    }

    bool ReadAndWriteOfAPC::InitiateAPCMetaHeader(
        uint16_t total_capacity,
        APCGroupReserver::APCInitialIdentityStruct& container_configuration,
        const LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier& layout_weight,
        const SchemaDefinition::InitialRegionalDtypeConf& dtype_conf,
        const SchemaDefinition::InitialRegionalProtocol& protocol_conf,
        uint8_t version
    ) noexcept
    {
        HeaderOrchestrator::APCMetaBuffer header_meta_buffer{};

        if (!HeaderOrchestrator::InitializeDefaultHeaderBuffer(
            header_meta_buffer,
            container_configuration,
            total_capacity,
            layout_weight,
            dtype_conf,
            protocol_conf,
            version
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


}