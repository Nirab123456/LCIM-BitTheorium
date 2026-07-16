#pragma once
#include <functional>
#include "CompleateRegionOrchestrator.hpp"

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
            APCMetaBuffer& header
        ) noexcept
        {
            header[static_cast<size_t>(HeaderIdentifierOfAPC::APC_SLOT_IDX)] = identity_cfg.APCSlotIndex;
            header[static_cast<size_t>(HeaderIdentifierOfAPC::BRANCH_ID)] = identity_cfg.BranchID;
            header[static_cast<size_t>(HeaderIdentifierOfAPC::LOGICAL_GROUP_ID)] = identity_cfg.LogicalGroupId;
            header[static_cast<size_t>(HeaderIdentifierOfAPC::SHARED_GROUP_ID)] = identity_cfg.SharedGroupId;
            header[static_cast<size_t>(HeaderIdentifierOfAPC::SHARED_ID_HASH_KEY)] = identity_cfg.SharedHashKey;
            header[static_cast<size_t>(HeaderIdentifierOfAPC::LOGICAL_ID_HASH_KEY)] = identity_cfg.LogicalHashKey;
            header[static_cast<size_t>(HeaderIdentifierOfAPC::NEXT_HORIZONTAL_HANDLE)] = identity_cfg.SharedNextHandle;
            header[static_cast<size_t>(HeaderIdentifierOfAPC::PREVIOUS_HORIZONTAL_HANDLE)] = identity_cfg.SharedPreviousHandle;
            header[static_cast<size_t>(HeaderIdentifierOfAPC::NEXT_VERTICAL_HANDLE)] = identity_cfg.LogicalNextHandle;
            header[static_cast<size_t>(HeaderIdentifierOfAPC::PREVIOUS_VERTICAL_HANDLE)] = identity_cfg.LogicalPreviousHandle;
            header[static_cast<size_t>(HeaderIdentifierOfAPC::TOTAL_HORIZONTAL_COUNT_S)] = identity_cfg.SharedSequentialCount;
            header[static_cast<size_t>(HeaderIdentifierOfAPC::TOTAL_VERTICAL_COUNT_L)] = identity_cfg.LogicalSequentalCount;
            header[static_cast<size_t>(HeaderIdentifierOfAPC::ACCESS_PASSWORD)] = identity_cfg.AccessPassword;
        }

        // static constexpr bool InitializeDefaultHeaderBuffer(
        //     APCMetaBuffer& return_buffer,
        //     APCGroupReserver::APCInitialIdentityStruct& provided_identity_cfg,
        //     uint16_t capacity_of_apc,
        //     const LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier& layour_weight = LayoutBoundsOrchestrator::DEFAULT_LAYOUT_WEIGHT,
        //     const SchemaDefinition::InitialRegionalDtypeConf& dtype_conf = SchemaDefinition::InitialRegionalDtypeConf{},
        //     const SchemaDefinition::InitialRegionalProtocol& protocol_conf = SchemaDefinition::InitialRegionalProtocol{},
        //     uint8_t version = APCDataStructure::BRANCH_VERSION
        // ) noexcept
        // {

        // }


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