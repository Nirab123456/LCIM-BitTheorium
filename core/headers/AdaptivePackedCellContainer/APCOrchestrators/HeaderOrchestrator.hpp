#pragma once
#include <functional>
#include "LayoutBoundsOrchestrator.hpp"

namespace PredictedAdaptedEncoding
{


    struct HeaderOrchestrator
    {
        #define MAXIMUM_CLAIMABLE_COUNT_SEQUENTIALLY 32
        static constexpr uint8_t LEN_OF_APC_META_BUFFER_OR_COUNT = APCDataStructure::METACELL_COUNT + 1;


        using DefaultMemCopyBuffer = std::array<packed64_t, MAXIMUM_CLAIMABLE_COUNT_SEQUENTIALLY>;

        using APCMetaBuffer = std::array<packed64_t, LEN_OF_APC_META_BUFFER_OR_COUNT>;


        static constexpr void BuildNullMemCopyBuffer(DefaultMemCopyBuffer& a_default_buffer) noexcept
        {
            for (size_t i = 0; i < a_default_buffer.size(); i++)
            {
                a_default_buffer[i] = PackedCell64_t::PACKED_CELL_SENTINAL;
            }
        }

        static constexpr void ConstructNullHeaderBuffer(APCMetaBuffer& a_meta_buffer) noexcept
        {
            for (size_t i = 0; i < a_meta_buffer.size(); i++)
            {
                a_meta_buffer[i] = PackedCell64_t::PACKED_CELL_SENTINAL;
            }
        }

        static constexpr void InsertTypedValue48MetaInBuffer(
            MetaIndexOfAPCNode meta_idx, 
            uint64_t value48,
            APCMetaBuffer& a_header_buffer,
            LocalityPolicy locality = LocalityPolicy::IDLE
        ) noexcept
        {
            const size_t idx_of_meta = static_cast<size_t>(meta_idx);

            const packed64_t packed_cell = PackedCell64_t::MakeTypedAPCValidPackedCell(
                TypeFamily::VALUE48,
                AccessContractOfValue::CAS_RMW,
                APCPagedNodeSegmentClasses::META_HEADER,
                locality,
                InternalDataTypePolicy::UnsignedPCellDataType,
                AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL,
                value48,
                UNSIGNED_ZERO
            );
            
            if (packed_cell == PackedCell64_t::PACKED_CELL_SENTINAL)
            {
                return;
            }
            
            a_header_buffer[idx_of_meta] = packed_cell;
        }

        static constexpr void ConfigureThisMetaBufferIdentity(
            const APCGroupReserver::APCInitialIdentityStruct& identity_cfg,
            APCMetaBuffer& a_header_buffer,
            LocalityPolicy locality = LocalityPolicy::PUBLISHED
        ) noexcept
        {
            InsertTypedValue48MetaInBuffer(MetaIndexOfAPCNode::APC_SLOT_IDX, identity_cfg.APCSlotIndex, a_header_buffer, locality);
            InsertTypedValue48MetaInBuffer(MetaIndexOfAPCNode::BRANCH_ID, identity_cfg.BranchID, a_header_buffer, locality);
            InsertTypedValue48MetaInBuffer(MetaIndexOfAPCNode::LOGICAL_GROUP_ID, identity_cfg.LogicalId, a_header_buffer, locality);
            InsertTypedValue48MetaInBuffer(MetaIndexOfAPCNode::SHARED_GROUP_ID, identity_cfg.SharedID, a_header_buffer, locality);

            InsertTypedValue48MetaInBuffer(MetaIndexOfAPCNode::SHARED_ID_HASH_KEY, identity_cfg.SharedHashKey, a_header_buffer, locality);
            InsertTypedValue48MetaInBuffer(MetaIndexOfAPCNode::LOGICAL_ID_HASH_KEY, identity_cfg.LogicalHashKey, a_header_buffer, locality);

            InsertTypedValue48MetaInBuffer(MetaIndexOfAPCNode::NEXT_HORIZONTAL_S, identity_cfg.SharedNextId, a_header_buffer, locality);
            InsertTypedValue48MetaInBuffer(MetaIndexOfAPCNode::PREVIOUS_HORIZONTAL_S, identity_cfg.SharedPreviousId, a_header_buffer, locality);
            InsertTypedValue48MetaInBuffer(MetaIndexOfAPCNode::NEXT_VERTICAL_L, identity_cfg.LogicalNextId, a_header_buffer, locality);
            InsertTypedValue48MetaInBuffer(MetaIndexOfAPCNode::PREVIOUS_VERTICAL_L, identity_cfg.LogicalPreviousId, a_header_buffer, locality);

            InsertTypedValue48MetaInBuffer(MetaIndexOfAPCNode::TOTAL_HORIZONTAL_COUNT_S, identity_cfg.SharedSequentialCount, a_header_buffer, locality);
            InsertTypedValue48MetaInBuffer(MetaIndexOfAPCNode::TOTAL_VERTICAL_COUNT_L, identity_cfg.LogicalSequentalCount, a_header_buffer, locality);
            
            InsertTypedValue48MetaInBuffer(MetaIndexOfAPCNode::ACCESS_PASSWORD, identity_cfg.AccessPassword, a_header_buffer, locality);
        }


        static constexpr void InitializeDefaultHeaderBuffer(APCGroupReserver::APCIdentityDef& required_identity);

        static constexpr bool ValidateAHeaderBuffer(APCMetaBuffer& a_header_buffer) noexcept;




    };
    

}