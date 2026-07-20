#pragma once
#include <functional>
#include "CompleateRegionOrchestrator.hpp"

namespace PredictedAdaptedEncoding
{


    struct HeaderOrchestrator
    {
        static constexpr uint64_t COMPLETE_HEADER_VALIDATION_MARK = 3333;
        static constexpr uint8_t LEN_OF_APC_META_BUFFER_OR_COUNT = APCDataStructure::METACELL_COUNT + 1;
        static constexpr uint8_t VALIDATION_INDEX_OF_HEADER_BUFFER = LEN_OF_APC_META_BUFFER_OR_COUNT - 1;


        using DefaultMemCopyBuffer = std::array<uint64_t, UINT8_MAX>;

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


        static constexpr bool InitializeDefaultHeaderBuffer(
            APCMetaBuffer& header_buffer,
            APCGroupReserver::APCInitialIdentityStruct& identity_apc,
            uint32_t capacity_of_apc,
            const LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier& layout_weight = LayoutBoundsOrchestrator::DEFAULT_LAYOUT_WEIGHT,
            const SchemaDefinition::InitialRegionalDtypeConf& dtype_conf = SchemaDefinition::InitialRegionalDtypeConf{},
            const SchemaDefinition::InitialRegionalProtocol& protocol_conf = SchemaDefinition::InitialRegionalProtocol{},
            uint8_t version = APCDataStructure::BRANCH_VERSION
        ) noexcept
        {
            for (uint64_t& word : header_buffer)
            {
                word  = UNSIGNED_ZERO;
            }

            if (
                !APCGroupReserver::IfSystemResolvedIdentityValid(identity_apc) ||
                !APCDataStructure::IsCapacityOfAPCValid(capacity_of_apc) ||
                !APCDataStructure::InLimitOfUint8(version)
            )
            {
                return false;
            }
            
            BufferConfForTracking::TrackingBufferOfAPC layout_buffer{};
            BufferConfForTracking::TrackingBufferOfAPC schema_buffer{};
            CompleateRegionOrchestrator::CursorBuffers cursors_buffrers{};

            bool layout_ok = LayoutBoundsOrchestrator::BuildInitialLayoutBuffer(
                layout_buffer,
                capacity_of_apc,
                layout_weight
            );

            bool schema_ok = CompleateRegionOrchestrator::BuildInitialSchemaBuffer(
                schema_buffer,
                layout_buffer,
                version,
                dtype_conf,
                protocol_conf
            );

            bool cursors_ok = CompleateRegionOrchestrator::BuildInitialCursorBuffers(
                cursors_buffrers,
                schema_buffer
            );

            if (!layout_ok || !schema_ok || !cursors_ok)
            {
                return false;
            }
            
            if (
                !CompleateRegionOrchestrator::ValidateSchemaBufferAgainstLayoutBuffer(schema_buffer, layout_buffer) ||
                !CompleateRegionOrchestrator::ValidateInitialCursorBuffers(cursors_buffrers, schema_buffer)
            )
            {
                return false;
            }
            CopyAllBuffersToHeaderBuffer_(header_buffer, layout_buffer, schema_buffer, cursors_buffrers);
            header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::MAGIC_ID)] = APCDataStructure::BRANCH_MAGIC;
            header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::SEGMENT_CONF_FLAGS)] = UNSIGNED_ZERO;
            header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::CAPACITY)] = capacity_of_apc;
            header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::APC_SCHEMA_ID)] = CompleateRegionOrchestrator::ComputeAPCSchemaId(layout_buffer, schema_buffer);

            header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::LAYOUT_VERSION)] = version;
            header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::LAYOUT_MUTATION_EPOCH)] = 0u;
            header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::LAYOUT_FLAGS)] = 0u;
            header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::NODE_GROUP_SIZE)] = 1u;

            header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::EOF_APC_HEADER)] = APCDataStructure::EOF_HEADER;


            return ValidateHeaderBuffer(header_buffer);

        }

        static constexpr bool ValidateFabricResolvedIdentity(APCMetaBuffer& header_buffer) noexcept
        {
            bool ok = HashIdConstructror::IsValidAPCSlotIdx(header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::APC_SLOT_IDX)]) &&
                HashIdConstructror::IsValidAPCId(header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::BRANCH_ID)]) &&
                HashIdConstructror::IsValidAPCId(header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::ACCESS_PASSWORD)]);
            if (!ok)
            {
                header_buffer[VALIDATION_INDEX_OF_HEADER_BUFFER] = UNSIGNED_ZERO;
                return false;
            }
            return true;
        }

        static constexpr bool ValidateHeaderBuffer(
            APCMetaBuffer& header
        ) noexcept
        {
            if (
                header[static_cast<size_t>(HeaderIdentifierOfAPC::MAGIC_ID)] != APCDataStructure::BRANCH_MAGIC ||
                header[static_cast<size_t>(HeaderIdentifierOfAPC::EOF_APC_HEADER)] != APCDataStructure::EOF_HEADER
            )
            {
                header[VALIDATION_INDEX_OF_HEADER_BUFFER] = UNSIGNED_ZERO;
                return false;
            }
            BufferConfForTracking::TrackingBufferOfAPC layout_buffer{};
            BufferConfForTracking::TrackingBufferOfAPC schema_buffer{};
            CompleateRegionOrchestrator::CursorBuffers enq_dq_cursors{};

            BufferConfForTracking::BuildNullTrackingBuffer(layout_buffer);
            BufferConfForTracking::BuildNullTrackingBuffer(schema_buffer);
            BufferConfForTracking::BuildNullTrackingBuffer(enq_dq_cursors.Enqueue);
            BufferConfForTracking::BuildNullTrackingBuffer(enq_dq_cursors.Dequeue);

            GetAllBuffersFromHeaderBuffer_(
                header,
                layout_buffer,
                schema_buffer,
                enq_dq_cursors
            );

            const uint32_t total_64Bit_unit_count_in_apc = static_cast<uint32_t>(header[static_cast<size_t>(HeaderIdentifierOfAPC::CAPACITY)]);
            
            bool layout_ok = LayoutBoundsOrchestrator::ValidateALayoutBuffer(layout_buffer, total_64Bit_unit_count_in_apc);
            
            bool schema_ok = CompleateRegionOrchestrator::ValidateSchemaBufferAgainstLayoutBuffer(schema_buffer, layout_buffer);

            bool enq_dq_ok = CompleateRegionOrchestrator::ValidateInitialCursorBuffers(enq_dq_cursors, schema_buffer);

            if (
                !ValidateFabricResolvedIdentity(header) ||
                !layout_ok || !schema_ok || !enq_dq_ok ||
                header[static_cast<size_t>(HeaderIdentifierOfAPC::APC_SCHEMA_ID)] != 
                    CompleateRegionOrchestrator::ComputeAPCSchemaId(layout_buffer, schema_buffer)
            )
            {
                header[VALIDATION_INDEX_OF_HEADER_BUFFER] = UNSIGNED_ZERO;
                return false;
            }
            
            header[VALIDATION_INDEX_OF_HEADER_BUFFER] = COMPLETE_HEADER_VALIDATION_MARK;
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

    
    private:
        static constexpr void CopyAllBuffersToHeaderBuffer_(
            APCMetaBuffer& header_buffer,
            const BufferConfForTracking::TrackingBufferOfAPC& layout_buffer,
            const BufferConfForTracking::TrackingBufferOfAPC& schema_buffer,
            const CompleateRegionOrchestrator::CursorBuffers& cursors_buffrers
        ) noexcept
        {
            for (uint8_t i = 0; i < APCDataStructure::CountOfMacroColumn(); i++)
            {
                header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::FEEDFORWARD_BOUNDS) + i] = layout_buffer[i];

                header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::FEEDFORWARD_ENQUEUE_POSITION) + i] = cursors_buffrers.Enqueue[i];

                header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::FEEDFORWARD_DEQUEUE_POSITION) + i] = cursors_buffrers.Dequeue[i];

                header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::FEEDFORWARD_REGION_SCHEMA) + i] = schema_buffer[i];
            }
            
        }


        static constexpr void GetAllBuffersFromHeaderBuffer_(
            const APCMetaBuffer& header_buffer,
            BufferConfForTracking::TrackingBufferOfAPC& layout_buffer,
            BufferConfForTracking::TrackingBufferOfAPC& schema_buffer,
            CompleateRegionOrchestrator::CursorBuffers& cursors_buffrers
        ) noexcept
        {
            for (uint8_t i = 0; i < APCDataStructure::CountOfMacroColumn(); i++)
            {
                layout_buffer[i] = header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::FEEDFORWARD_BOUNDS) + i];
                schema_buffer[i] = header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::FEEDFORWARD_REGION_SCHEMA) + i];
                cursors_buffrers.Enqueue[i] = header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::FEEDFORWARD_ENQUEUE_POSITION) + i];
                cursors_buffrers.Dequeue[i] = header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::FEEDFORWARD_DEQUEUE_POSITION) + i];

            }
            
        }



    };
    

}