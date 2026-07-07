#pragma once
#include <functional>
#include "FabricToAPCLinker.hpp"

namespace PredictedAdaptedEncoding
{
    class ReadAndWriteOfAPC : public FabricToAPCLinker
    {

    protected:
        bool ReadCompleateMetaHeaderDirectlyNonAtomic_(HeaderOrchestrator::APCMetaBuffer& a_default_buffer) noexcept;

        bool ReadCompleatLayoutBuffer_(
            LayoutBoundsOrchestrator::TrackingBufferOfAPC& a_layout_buffer,
            bool is_claimed_required = false
        ) noexcept;

        bool UpdateCompleateLayoutOfAPCFromBuffer_(
            const LayoutBoundsOrchestrator::TrackingBufferOfAPC& a_valid_layout_buffer,
            bool caller_holds_the_flag = false
        ) noexcept;

    public:
        bool InitiateAPCMetaHeader(
            uint16_t total_capacity,
            APCGroupReserver::APCInitialIdentityStruct& container_configuration,
            const LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier& user_defined_weight = LayoutBoundsOrchestrator::DEFAULT_LAYOUT_WEIGHT,
            uint8_t version = APCDataStructure::BRANCH_VERSION,
            LocalityPolicy locality = LocalityPolicy::PUBLISHED
        ) noexcept;

        ////// SINGLE READ WRITE/////

        /// @return The Update only ContractOfConcurrency::BOUNDED_RETRY_CAS_NO_CLAIMED
        uint64_t AtomicallyUpdateACounterFromAPC(uint16_t desired_idx, uint32_t delta) noexcept;

        uint64_t AtomicallyUpdateMetaCellCounter(MetaIndexOfAPCNode meta_idx, uint32_t delta) noexcept;


    };
    
    
}