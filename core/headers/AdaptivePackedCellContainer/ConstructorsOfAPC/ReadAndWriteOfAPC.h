#pragma once
#include <functional>
#include "FabricToAPCLinker.hpp"

namespace BidirectionalInMemGraph
{
    class ReadAndWriteOfAPC : public FabricToAPCLinker
    {

    protected:

        bool ReadCompleatLayoutBuffer_(
            LayoutBoundsOrchestrator::TrackingBufferOfAPC& a_layout_buffer
        ) noexcept;

    public:


        bool InitiateAPCMetaHeader(
            const LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier& user_defined_weight = LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier{},
            const SchemaDefinition::InitialRegionalDtypeConf& dtype_conf = SchemaDefinition::InitialRegionalDtypeConf{},
            const SchemaDefinition::InitialRegionalProtocol& protocol_conf = SchemaDefinition::InitialRegionalProtocol{},
            uint8_t version = APCDataStructure::BRANCH_VERSION
        ) noexcept;


        bool ReadAPCMetaUnit(
            HeaderIdentifierOfAPC meta_idx,
            uint64_t& return_value,
            bool atomic_required = true
        ) noexcept;

        bool CompareExchangeAPCMetaUinit(
            HeaderIdentifierOfAPC meta_idx,
            uint64_t& expected_value,
            uint64_t desired_value
        ) noexcept;


    };
    
    
}