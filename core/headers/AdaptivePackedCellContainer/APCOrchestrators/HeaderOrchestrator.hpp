#pragma once
#include <functional>
#include "CompleateRegionOrchestrator.hpp"

namespace BidirectionalInMemGraph
{

    struct DescriptionOfAPC 
    {

        using APCDescriptorRange = RangeOfAPC;
        
        struct SeqLockAndStateStruct
        {
            uint32_t SeqLock = UINT32_MAX;
            StateOfAPC StateOfTheAPC = StateOfAPC::RETIRED;
            bool IsValid = false;
        };

        static_assert(sizeof(SeqLockAndStateStruct) <= sizeof(uint64_t));

        static constexpr uint64_t ComposeSeqLockAndState(SeqLockAndStateStruct& files) noexcept
        {
            if (
                !APCDataStructure::IsValid32BitAPCUnit(files.SeqLock) ||
                !ValidateStateAgainstSeqLock(files)
            )
            {
                return FABRIC_CELL_SENTINAL;
            }
            return TwinU32ToU64::PackDoubleUnsigned32In64(files.SeqLock, static_cast<uint32_t>(files.StateOfTheAPC));
        }

        static constexpr bool GetSeqLockAndLifeCycle(
            uint64_t desc_id_state,
            SeqLockAndStateStruct& values
        ) noexcept
        {
            values = SeqLockAndStateStruct{};
            values.SeqLock = TwinU32ToU64::ExtractLow32Of64(desc_id_state);
            values.StateOfTheAPC = static_cast<StateOfAPC>(TwinU32ToU64::ExtractHigh32Of64(desc_id_state));

            if (!APCDataStructure::IsValidFabricUnit(values.SeqLock))
            {
                return false;
            }
            return ValidateStateAgainstSeqLock(values);
        }

        static constexpr bool ValidateStateAgainstSeqLock(SeqLockAndStateStruct& files) noexcept
        {
            if (!APCDataStructure::IsValidFabricUnit(files.SeqLock))
            {
                files.IsValid = false;
                return false;
            }
            if (
                files.StateOfTheAPC == StateOfAPC::RESERVED &&
                InstallAxisToBuffer::IsValidEven64(files.SeqLock)
            )
            {
                files.IsValid = false;
                return false;
            }
            if (
                files.StateOfTheAPC != StateOfAPC::RESERVED &&
                !InstallAxisToBuffer::IsValidEven64(files.SeqLock)
            )
            {
                files.IsValid = false;
                return false;
            }

            files.IsValid = true;
            return true;
        }

        static constexpr bool IsTransitionStateLeagal(StateOfAPC current_state, StateOfAPC desired_state) noexcept
        {
            return (current_state == StateOfAPC::FREE && desired_state == StateOfAPC::RESERVED) ||
                (current_state == StateOfAPC::RESERVED && desired_state == StateOfAPC::FREE) ||
                (current_state == StateOfAPC::RESERVED && desired_state == StateOfAPC::LIVE) ||
                (current_state == StateOfAPC::LIVE && desired_state == StateOfAPC::RESERVED) ||
                (current_state == StateOfAPC::RESERVED && desired_state == StateOfAPC::RETIRED) ||
                (current_state == StateOfAPC::RETIRED && desired_state == StateOfAPC::RESERVED) ||
                (current_state == StateOfAPC::LIVE && desired_state == StateOfAPC::HAULTED) ||
                (current_state == StateOfAPC::HAULTED && desired_state == StateOfAPC::LIVE);
                
        }

    };

    struct HeaderOrchestrator : DescriptionOfAPC
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

        static constexpr bool ConfigureThisMetaBufferIdentity(
           InstallAxisToBuffer::BufferOfAPCIdentity identity_buffer,
            APCMetaBuffer& header
        ) noexcept
        {
            if (!InstallAxisToBuffer::ValidateIdentityBuffer(identity_buffer))
            {
                return false;
            }

            for (uint8_t i = 0; i < APCDataStructure::TotalIdentityUnitCount(); i++)
            {
                const HeaderIdentifierOfAPC identity = InstallAxisToBuffer::GetIdentityUnitFromBufferIdx(i).value();
                header[static_cast<uint8_t>(identity)] = identity_buffer[i];
            }
            return true;
        }


        static constexpr bool InitializeDefaultHeaderBuffer(
            APCMetaBuffer& header_buffer,
            InstallAxisToBuffer::BufferOfAPCIdentity& identity_buffer,
            uint32_t capacity_of_apc,
            const LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier& layout_weight = LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier{},
            const SchemaDefinition::InitialRegionalDtypeConf& dtype_conf = SchemaDefinition::InitialRegionalDtypeConf{},
            const SchemaDefinition::InitialRegionalProtocol& protocol_conf = SchemaDefinition::InitialRegionalProtocol{},
            uint8_t version = APCDataStructure::BRANCH_VERSION
        ) noexcept
        {
            SeqLockAndStateStruct values{};
            if (
                !GetSeqLockAndLifeCycle(header_buffer[static_cast<uint8_t>(HeaderIdentifierOfAPC::APC_LIFE_CYCLE)], values) ||
                values.StateOfTheAPC != StateOfAPC::RESERVED
            )
            {
                return false;
            }

            values.StateOfTheAPC = StateOfAPC::LIVE;
            ++values.SeqLock;
            const uint64_t raw_new_state_seq = ComposeSeqLockAndState(values);
            

            for (uint64_t& word : header_buffer)
            {
                word  = UNSIGNED_ZERO;
            }

            if (
                !ConfigureThisMetaBufferIdentity(identity_buffer, header_buffer)||
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
                !CompleateRegionOrchestrator::ValidateInitialCursorBuffers(cursors_buffrers, schema_buffer) ||
                !APCDataStructure::IsValidFabricUnit(raw_new_state_seq)
            )
            {
                return false;
            }
            CopyAllBuffersToHeaderBuffer_(header_buffer, layout_buffer, schema_buffer, cursors_buffrers);
            header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::MAGIC_ID)] = APCDataStructure::BRANCH_MAGIC;
            header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::APC_SCHEMA_ID)] = CompleateRegionOrchestrator::ComputeAPCSchemaId(layout_buffer, schema_buffer);
            header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::LAYOUT_VERSION)] = version;
            header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::APC_LIFE_CYCLE)] = raw_new_state_seq;
            header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::EOF_APC_HEADER)] = APCDataStructure::EOF_HEADER;


            return ValidateHeaderBuffer(header_buffer);

        }

        static constexpr bool ValidateFabricResolvedIdentity(APCMetaBuffer& header_buffer) noexcept
        {
            bool ok = APCDataStructure::IsValid32BitAPCUnit(header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::APC_SLOT_IDX)]);
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
            InstallAxisToBuffer::BufferOfAPCIdentity identity_buffer{};

            BufferConfForTracking::BuildNullTrackingBuffer(layout_buffer);
            BufferConfForTracking::BuildNullTrackingBuffer(schema_buffer);
            BufferConfForTracking::BuildNullTrackingBuffer(enq_dq_cursors.Enqueue);
            BufferConfForTracking::BuildNullTrackingBuffer(enq_dq_cursors.Dequeue);
            InstallAxisToBuffer::BuildNullIdentityBuffer(identity_buffer);

            GetAllBuffersFromHeaderBuffer_(
                header,
                layout_buffer,
                schema_buffer,
                enq_dq_cursors,
                identity_buffer
            );

            bool identity_ok = InstallAxisToBuffer::ValidateIdentityBuffer(identity_buffer);

            const uint64_t total_64Bit_unit_count_in_apc = InstallAxisToBuffer::ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::BOUNDS_END) -
                InstallAxisToBuffer::ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::BOUNDS_BEGIN);

            bool layout_ok = LayoutBoundsOrchestrator::ValidateALayoutBuffer(layout_buffer, static_cast<uint32_t>(total_64Bit_unit_count_in_apc));
            
            bool schema_ok = CompleateRegionOrchestrator::ValidateSchemaBufferAgainstLayoutBuffer(schema_buffer, layout_buffer);

            bool enq_dq_ok = CompleateRegionOrchestrator::ValidateInitialCursorBuffers(enq_dq_cursors, schema_buffer);

            if (
                !identity_ok ||
                !layout_ok ||
                !schema_ok || 
                !enq_dq_ok ||
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
            CompleateRegionOrchestrator::CursorBuffers& cursors_buffrers,
            InstallAxisToBuffer::BufferOfAPCIdentity& identity_buffer
        ) noexcept
        {
            for (uint8_t i = 0; i < APCDataStructure::CountOfMacroColumn(); i++)
            {
                layout_buffer[i] = header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::FEEDFORWARD_BOUNDS) + i];
                schema_buffer[i] = header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::FEEDFORWARD_REGION_SCHEMA) + i];
                cursors_buffrers.Enqueue[i] = header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::FEEDFORWARD_ENQUEUE_POSITION) + i];
                cursors_buffrers.Dequeue[i] = header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::FEEDFORWARD_DEQUEUE_POSITION) + i];
            }

            for (size_t i = 0; i < APCDataStructure::TotalIdentityUnitCount(); i++)
            {
                identity_buffer[i] = header_buffer[static_cast<size_t>(HeaderIdentifierOfAPC::GRAPH_MUTATION_AND_LOCK) + i];
            }
            
            
        }



    };
    

}