#pragma once
#include <functional>
#include "FabricToAPCLinker.hpp"

namespace BidirectionalInMemGraph
{
    class ReadAndWriteOfAPC : public FabricToAPCLinker
    {

    protected:
        bool ReadCompleateMetaHeaderAtomically_(HeaderOrchestrator::APCMetaBuffer& a_default_buffer) noexcept;

        bool ReadCompleatLayoutBuffer_(
            LayoutBoundsOrchestrator::TrackingBufferOfAPC& a_layout_buffer,
            bool atomic_required = false
        ) noexcept;

    public:
        bool InitiateAPCMetaHeader(
            uint32_t total_capacity,
            InstallAxisToBuffer::BufferOfAPCIdentity& identity_buffer,
            const LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier& user_defined_weight = LayoutBoundsOrchestrator::DEFAULT_LAYOUT_WEIGHT,
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

        bool PublishIdentityBuffer(
            InstallAxisToBuffer::BufferOfAPCIdentity& desired_identity,
            uint32_t max_tries = DEFAULT_MAX_TRIES
        ) noexcept;

    };
    
    
}