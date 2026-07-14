#pragma once
#include <functional>
#include "MPMCOrchestratorForAPCRegion.hpp"

namespace PredictedAdaptedEncoding
{


    struct HeaderOrchestrator
    {
        #define MAXIMUM_CLAIMABLE_COUNT_SEQUENTIALLY 32

        static constexpr uint64_t COMPLETE_HEADER_VALIDATION_MARK = 3333;
        static constexpr uint8_t LEN_OF_APC_META_BUFFER_OR_COUNT = APCDataStructure::METACELL_COUNT + 1;
        static constexpr uint8_t VALIDATION_INDEX_OF_HEADER_BUFFER = LEN_OF_APC_META_BUFFER_OR_COUNT - 1;


        using DefaultMemCopyBuffer = std::array<uint64_t, MAXIMUM_CLAIMABLE_COUNT_SEQUENTIALLY>;

        using APCMetaBuffer = std::array<uint64_t, LEN_OF_APC_META_BUFFER_OR_COUNT>;


        static constexpr void BuildNullMemCopyBuffer(DefaultMemCopyBuffer& a_default_buffer) noexcept
        {
            for (size_t i = 0; i < a_default_buffer.size(); i++)
            {
                a_default_buffer[i] = FABRIC_CELL_SENTINAL;
            }
        }

        static constexpr void ConstructNullHeaderBuffer(APCMetaBuffer& a_meta_buffer) noexcept
        {
            for (size_t i = 0; i < a_meta_buffer.size(); i++)
            {
                a_meta_buffer[i] = FABRIC_CELL_SENTINAL;
            }
        }


        static constexpr void ConfigureThisMetaBufferIdentity(
            const APCGroupReserver::APCInitialIdentityStruct& identity_cfg,
            APCMetaBuffer& a_header_buffer,
            LocalityPolicy locality = LocalityPolicy::PUBLISHED
        ) noexcept
        {
            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::APC_SLOT_IDX, identity_cfg.APCSlotIndex, a_header_buffer, locality);
            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::BRANCH_ID, identity_cfg.BranchID, a_header_buffer, locality);
            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::LOGICAL_GROUP_ID, identity_cfg.LogicalId, a_header_buffer, locality);
            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::SHARED_GROUP_ID, identity_cfg.SharedID, a_header_buffer, locality);

            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::SHARED_ID_HASH_KEY, identity_cfg.SharedHashKey, a_header_buffer, locality);
            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::LOGICAL_ID_HASH_KEY, identity_cfg.LogicalHashKey, a_header_buffer, locality);

            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::NEXT_HORIZONTAL_HANDLE, identity_cfg.SharedNextHandle, a_header_buffer, locality);
            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::PREVIOUS_HORIZONTAL_HANDLE, identity_cfg.SharedPreviousHandle, a_header_buffer, locality);
            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::NEXT_VERTICAL_HANDLE, identity_cfg.LogicalNextHandle, a_header_buffer, locality);
            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::PREVIOUS_VERTICAL_HANDLE, identity_cfg.LogicalPreviousHandle, a_header_buffer, locality);

            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::TOTAL_HORIZONTAL_COUNT_S, identity_cfg.SharedSequentialCount, a_header_buffer, locality, ContractOfConcurrency::BOUNDED_RETRY_CAS_NO_CLAIMED);
            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::TOTAL_VERTICAL_COUNT_L, identity_cfg.LogicalSequentalCount, a_header_buffer, locality, ContractOfConcurrency::BOUNDED_RETRY_CAS_NO_CLAIMED);
            
            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::ACCESS_PASSWORD, identity_cfg.AccessPassword, a_header_buffer, locality);
        }

        static constexpr bool InitializeDefaultHeaderBuffer(
            APCMetaBuffer& return_buffer,
            APCGroupReserver::APCInitialIdentityStruct& provided_identity_cfg,
            uint16_t capacity_of_apc,
            const LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier& user_defined_weight = LayoutBoundsOrchestrator::DEFAULT_LAYOUT_WEIGHT,
            uint8_t version = APCDataStructure::BRANCH_VERSION,
            LocalityPolicy locality = LocalityPolicy::PUBLISHED
        ) noexcept
        {
            ConstructNullHeaderBuffer(return_buffer);

            if (
                !APCGroupReserver::IfSystemResolvedIdentityValid(provided_identity_cfg) ||
                !APCDataStructure::IsCapacityOfAPCValid(capacity_of_apc) ||
                !APCDataStructure::ThisVersionValid(version)
            )
            {
                return false;
            }

            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::MAGIC_ID, APCDataStructure::BRANCH_MAGIC, return_buffer, locality);
            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::SEGMENT_CONF_FLAGS, UNSIGNED_ZERO, return_buffer, locality);
            ConfigureThisMetaBufferIdentity(provided_identity_cfg, return_buffer, locality);

            if (!InitiateLayoutThenOccupencyInHeaderBuffer(
                return_buffer,
                capacity_of_apc,
                user_defined_weight,
                version
            ))
            {
                return false;
            }

            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::BRANCH_PRIORITY, UNSIGNED_ZERO, return_buffer, locality);
            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::SEGMENT_CONF_FLAGS, UNSIGNED_ZERO, return_buffer, locality);
            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::SPLIT_THRESHOLD_PERCENTAGE, UNSIGNED_ZERO, return_buffer, locality);
            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::TOTAL_CAPACITY_OF_THIS_SEGEMENT, capacity_of_apc, return_buffer, locality);
            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::PAGED_NODE_READY_BIT, UNSIGNED_ZERO, return_buffer, locality);
            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::DEFINED_MODE_OF_CURRENT_APC, static_cast<uint64_t>(provided_identity_cfg.InitialMode), return_buffer, locality);
            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::PRODUCER_CURSOR_PLACEMENT, UNSIGNED_ZERO, return_buffer, locality);
            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::CONSUMER_CURSORE_PLACEMENT, UNSIGNED_ZERO, return_buffer, locality);
            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::TOTAL_CAS_FAILURE_FOR_THIS_APC_BRANCH, UNSIGNED_ZERO, return_buffer, locality);
            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::NODE_GROUP_SIZE, 1u, return_buffer, locality);

            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::LOCAL_CLOCK48, UNSIGNED_ZERO, return_buffer, locality);
            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::LAST_ACCEPTED_FEED_FORWARD_CLOCK16, UNSIGNED_ZERO, return_buffer, locality);
            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::LAST_EMITTED_FEED_FORWARD_CLOCK16, UNSIGNED_ZERO, return_buffer, locality);
            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::LAST_ACCEPTED_FEED_BACKWARD_CLOCK16, UNSIGNED_ZERO, return_buffer, locality);
            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::LAST_EMITTED_FEED_BACKWARD_CLOCK16, UNSIGNED_ZERO, return_buffer, locality);
            InsertTypedValue48MetaInBuffer(HeaderIdentifierOfAPC::EOF_APC_HEADER, APCDataStructure::EOF_HEADER, return_buffer, locality);

            return_buffer[VALIDATION_INDEX_OF_HEADER_BUFFER] = COMPLETE_HEADER_VALIDATION_MARK;            
            return true;
        }


        static constexpr bool IsHeaderBufferValidationMarked(const APCMetaBuffer& a_buffer) noexcept
        {
            if (a_buffer[VALIDATION_INDEX_OF_HEADER_BUFFER] == COMPLETE_HEADER_VALIDATION_MARK)
            {
                return true;
            }
            return false;
        }





    };
    

}