#pragma once
#include "SchemaOrchestratorForRegion.hpp"

namespace PredictedAdaptedEncoding
{

    struct SchemaDefinition : SchemDefinition
    {
        static constexpr bool MakeInitialRegionSchema(
            RegionSchemaRecord& return_schema,
            uint32_t layout_span,
            const InitialRegionalDtypeConf& init_dtype = InitialRegionalDtypeConf{},
            const InitialRegionalProtocol& protocol_conf_init = InitialRegionalProtocol{}
        ) noexcept
        {
            if (!HasEnoughForInitialSchema(return_schema))
            {
                return_schema.IsValidSchema = false;
                return false;
            }
            
            return_schema.Dtype = GetDataTypeForColumn(init_dtype, return_schema.ParentColumn);
            return_schema.Protocol = GetProtocolForColumn(protocol_conf_init, return_schema.ParentColumn);

            return_schema.Flags = SchemaFlags::NONE;
            return_schema.RequiredTypedElementsPerRecord = UNSIGNED_ZERO;
            
            std::optional<uint8_t> equivelent_typed_count_of_64bit = CountOfTypedWordIn64Bit(return_schema.Dtype);

            if (
                !equivelent_typed_count_of_64bit.has_value() ||
                !IsKnownProtocol(return_schema.Protocol)
            )
            {
                return_schema.IsValidSchema = false;
                return false;
            }
            
            if (layout_span == UNSIGNED_ZERO)
            {
                return_schema.Flags = SchemaFlags::REGION_DISABLED;
                return_schema.IsValidSchema = true;
                return true;
            }

            const uint32_t half_count_of_layout64bit = layout_span / 2u;

            switch (return_schema.Protocol)
            {
            case SchemaProtocols::MPMC_FIXED_RECORD_QUEUE:
                return_schema.RequiredTypedElementsPerRecord = equivelent_typed_count_of_64bit.value();
                return_schema.Flags = SchemaFlags::REQUIRED_POW_OF_TWO |
                    SchemaFlags::ALLOW_TRAILING_PADDING |
                    SchemaFlags::HAS_PER_SLOT_SEQUENSE;
                return_schema.IsValidSchema = true;
                return true;
            
            case SchemaProtocols::ATOMIC_WORD_ARRAY:
                return_schema.RequiredTypedElementsPerRecord = equivelent_typed_count_of_64bit.value();
                return_schema.IsValidSchema = true;
                return true;

            case SchemaProtocols::DOUBLE_BUFFERED:
                if (half_count_of_layout64bit == UNSIGNED_ZERO)
                {
                    return_schema.IsValidSchema = false;
                    return false;
                }
                return_schema.RequiredTypedElementsPerRecord = half_count_of_layout64bit * equivelent_typed_count_of_64bit.value();
                if (
                    (layout_span % 2u) != UNSIGNED_ZERO
                )
                {
                    return_schema.Flags = SchemaFlags::ALLOW_TRAILING_PADDING;
                }
                return_schema.IsValidSchema = true;
                return true;

            case SchemaProtocols::PRIVATE_REGION:
            case SchemaProtocols::IMMUTABLE_SNAPSHOT:
                return_schema.RequiredTypedElementsPerRecord = layout_span * equivelent_typed_count_of_64bit.value();
                return_schema.IsValidSchema = true;
                return true;
                
            default:
                return_schema.IsValidSchema = false;
                return false;
            }
        }

        static constexpr bool ValidateSchemaAgainstLayout(
            RegionSchemaRecord& schema,
            uint32_t layout_span
        ) noexcept
        {
            if (
                !SchemaSelfValidation(schema) ||
                !schema.IsValidSchema
            )
            {
                schema.IsValidSchema = false;
                return false;
            }

            if (HasSchemaFlag(schema.Flags, SchemaFlags::REGION_DISABLED))
            {
                schema.IsValidSchema = layout_span == UNSIGNED_ZERO &&
                    schema.RequiredTypedElementsPerRecord == UNSIGNED_ZERO;
                return schema.IsValidSchema;
            }

            const std::optional<uint32_t> payload_size_in64bit_cell = Required64BitCellForDesiredPayload(schema);
            const std::optional<uint32_t> phy_64bit_on_protocol = CountOf64BitBasedOnTypedProtocol(schema);

            if (
                !payload_size_in64bit_cell.has_value() ||
                !phy_64bit_on_protocol.has_value() ||
                phy_64bit_on_protocol.value() == UNSIGNED_ZERO
            )
            {
                schema.IsValidSchema = false;
                return false;
            }
            
            switch (schema.Protocol)
            {
            case SchemaProtocols::MPMC_FIXED_RECORD_QUEUE:
            {
                if (
                    !HasSchemaFlag(schema.Flags, SchemaFlags::REQUIRED_POW_OF_TWO) ||
                    !HasSchemaFlag(schema.Flags, SchemaFlags::HAS_PER_SLOT_SEQUENSE)
                )
                {
                    schema.IsValidSchema = false;
                    return false;
                }

                const uint32_t raw64bit_slots = layout_span / phy_64bit_on_protocol.value();
                const uint32_t useable_slots = FloorPoweOfTwoUnsigned32(raw64bit_slots);

                if (!IsValuePowOfTwoU32(useable_slots))
                {
                    schema.IsValidSchema = false;
                    return false;
                }
                
                const uint32_t used_64bit_cell = useable_slots * phy_64bit_on_protocol.value();

                schema.IsValidSchema = used_64bit_cell == layout_span ||
                    (
                        used_64bit_cell < layout_span &&
                        HasSchemaFlag(schema.Flags, SchemaFlags::ALLOW_TRAILING_PADDING)
                    );

                return schema.IsValidSchema;
            }
            case SchemaProtocols::ATOMIC_WORD_ARRAY:
                schema.IsValidSchema = payload_size_in64bit_cell.value() == 1;
                return schema.IsValidSchema;
                
            case SchemaProtocols::DOUBLE_BUFFERED:
            {
                const uint32_t used_64bit_cell = payload_size_in64bit_cell.value() * 2u;

                schema.IsValidSchema =  used_64bit_cell == layout_span ||
                    (
                        used_64bit_cell < layout_span &&
                        HasSchemaFlag(schema.Flags, SchemaFlags::ALLOW_TRAILING_PADDING)
                    );
                return schema.IsValidSchema;
            }
            case SchemaProtocols::PRIVATE_REGION:
            case SchemaProtocols::IMMUTABLE_SNAPSHOT:
                schema.IsValidSchema = payload_size_in64bit_cell.value() == layout_span;
                return schema.IsValidSchema;
            
            default:
                schema.IsValidSchema = false;
                return false;
            }
            
            
        }

    };

    struct SchemaBufferOrchestrator : public BufferConfForTracking
    {
        static constexpr uint64_t VALIDATION_SCHEMA_MARK = 23333u;

        static constexpr bool HasSchemaBufferValidationMark(const TrackingBufferOfAPC& schema_buffer) noexcept
        {
            if (schema_buffer[VALIDATION_IDX_OF_TRACKING_BUFFER] == VALIDATION_SCHEMA_MARK)
            {
                return true;
            }
            return false;
        }

        static constexpr bool InsertASchemaInBuffer(
            TrackingBufferOfAPC& buffer_address,
            SchemaDefinition::RegionSchemaRecord& schema_record
        ) noexcept
        {

            const std::optional<uint8_t> buffer_idx = GetBufferIdxFromMacroColumn(schema_record.ParentColumn);
            if (!buffer_idx.has_value())
            {
                return false;
            }
                       
            uint64_t packed_schema = SchemaDefinition::PackRegionScheme(schema_record);
            if (!APCDataStructure::IsValidFabricUnit(packed_schema))
            {
                return false;
            }

            buffer_address[*buffer_idx] = packed_schema; 

            return true;
        }

        static constexpr bool BuildInitialSchemaBuffer (
            TrackingBufferOfAPC& return_schema_buffer,
            const TrackingBufferOfAPC& valid_layout_buffer,
            uint8_t version = APCDataStructure::BRANCH_VERSION,
            const SchemaDefinition::InitialRegionalDtypeConf& dtype_map = SchemaDefinition::InitialRegionalDtypeConf{},
            const SchemaDefinition::InitialRegionalProtocol& protocol_map = SchemaDefinition::InitialRegionalProtocol{}
        ) noexcept
        {
            BuildNullTrackingBuffer(return_schema_buffer);

            if (
                !LayoutBoundsOrchestrator::HasLayouBufferValidationMark(valid_layout_buffer) ||
                !APCDataStructure::InLimitOfUint8(version)
            )
            {
                return false;
            }
            
            for (uint8_t i = 0; i < APCDataStructure::CountOfMacroColumn(); i++)
            {
                const MacroColumnOfAPC column = GetMacroColumnFromBufferIdx(i);
                const std::optional<uint32_t> maybe_span = LayoutBoundsOrchestrator::SpanOflayoutFromPackedCell(valid_layout_buffer[i], column);

                if (!maybe_span.has_value())
                {
                    BuildNullTrackingBuffer(return_schema_buffer);
                    return false;
                }

                SchemaDefinition::RegionSchemaRecord schema{};
                schema.ParentColumn = column;
                schema.Version = version;
                bool schema_build_ok = SchemaDefinition::MakeInitialRegionSchema(
                    schema, maybe_span.value(),
                    dtype_map, protocol_map
                );

                bool insert_ok = InsertASchemaInBuffer(
                    return_schema_buffer,
                    schema
                );

                if (!schema_build_ok || !insert_ok)
                {
                    BuildNullTrackingBuffer(return_schema_buffer);
                    return false;
                }           
            }
            return_schema_buffer[VALIDATION_IDX_OF_TRACKING_BUFFER] = VALIDATION_SCHEMA_MARK;
            return true;     
        }

        static constexpr bool ValidateSchemaBufferAgainstLayoutBuffer(
            TrackingBufferOfAPC& valid_schema_buffer,
            const TrackingBufferOfAPC& valid_layout_buffer
        ) noexcept
        {
            if (
                !LayoutBoundsOrchestrator::HasLayouBufferValidationMark(valid_layout_buffer)
            )
            {
                valid_schema_buffer[VALIDATION_IDX_OF_TRACKING_BUFFER] = UNSIGNED_ZERO;
                return false;
            }
            
            uint8_t expected_version = UINT8_MAX;
            for (uint8_t i = 0; i < APCDataStructure::CountOfMacroColumn(); i++)
            {
                const MacroColumnOfAPC column = GetMacroColumnFromBufferIdx(i);
                const std::optional<uint32_t> maybe_span = LayoutBoundsOrchestrator::SpanOflayoutFromPackedCell(
                    valid_layout_buffer[i], column
                );

                SchemaDefinition::RegionSchemaRecord schema{};
                schema.ParentColumn = column;
                bool have_schema = SchemaDefinition::LayoutSchemaFromPackedCell(
                    schema,
                    valid_schema_buffer[i]
                );
                
                if (
                    !maybe_span.has_value() ||
                    !have_schema
                )
                {
                    valid_schema_buffer[VALIDATION_IDX_OF_TRACKING_BUFFER] = UNSIGNED_ZERO;
                    return false;
                }

                if (!SchemaDefinition::ValidateSchemaAgainstLayout(
                    schema,
                    maybe_span.value()
                ))
                {
                    valid_schema_buffer[VALIDATION_IDX_OF_TRACKING_BUFFER] = UNSIGNED_ZERO;
                    return false;
                }
                
                if (
                    APCDataStructure::InLimitOfUint8(schema.Version) &&
                    !APCDataStructure::InLimitOfUint8(expected_version)
                )
                {
                    expected_version = schema.Version;
                }

                if (
                    schema.Version != expected_version &&
                    APCDataStructure::InLimitOfUint8(expected_version)
                )
                {
                    valid_schema_buffer[VALIDATION_IDX_OF_TRACKING_BUFFER] = UNSIGNED_ZERO;
                    return false;
                }
            }
            valid_schema_buffer[VALIDATION_IDX_OF_TRACKING_BUFFER] = VALIDATION_SCHEMA_MARK;
            return true;
        }

        static constexpr uint64_t ComputeAPCSchemaId(
            const BufferConfForTracking::TrackingBufferOfAPC& layout,
            const BufferConfForTracking::TrackingBufferOfAPC& schema
        ) noexcept
        {
            uint64_t hash = 1469598103934665603ull;
            for (uint8_t i = 0u; i < APCDataStructure::CountOfMacroColumn(); ++i)
            {
                hash ^= layout[i];
                hash *= 1099511628211ull;
                hash ^= schema[i];
                hash *= 1099511628211ull;
            }
            return hash == FABRIC_CELL_SENTINAL ? hash - 1u : hash;
        }
    };

    struct CompleateRegionOrchestrator : public SchemaBufferOrchestrator
    {
        static constexpr uint64_t VALIDATION_CURSOR_BUFFER_MARK = 22222u;

        struct CursorBuffers
        {
            TrackingBufferOfAPC Enqueue{};
            TrackingBufferOfAPC Dequeue{};
        };


        static constexpr bool HasCursorBufferValidationMark(const TrackingBufferOfAPC& cursor_buffer) noexcept
        {
            if (cursor_buffer[VALIDATION_IDX_OF_TRACKING_BUFFER] == VALIDATION_CURSOR_BUFFER_MARK)
            {
                return true;
            }
            return false;
        }

        static constexpr bool BuildInitialCursorBuffers(
            CursorBuffers& buffers,
            const TrackingBufferOfAPC& valid_schema_buffer
        ) noexcept
        {
            BuildNullTrackingBuffer(buffers.Enqueue);
            BuildNullTrackingBuffer(buffers.Dequeue);

            if (!HasSchemaBufferValidationMark(valid_schema_buffer))
            {
                return false;
            }
            
            for (uint8_t i = 0; i < ColumnConf::CountOfMacroColumn(); i++)
            {
                const MacroColumnOfAPC column = GetMacroColumnFromBufferIdx(i);

                SchemaDefinition::RegionSchemaRecord schema{};
                schema.ParentColumn = column;

                if (
                    !SchemaDefinition::LayoutSchemaFromPackedCell
                    (
                    schema,
                    valid_schema_buffer[i]
                    )
                )
                {
                    return false;
                }

                if (
                    schema.Protocol == SchemaDefinition::SchemaProtocols::MPMC_FIXED_RECORD_QUEUE &&
                    !SchemaDefinition::HasSchemaFlag(schema.Flags, SchemaDefinition::SchemaFlags::REGION_DISABLED)
                )
                {
                    buffers.Enqueue[i] = UNSIGNED_ZERO;
                    buffers.Dequeue[i] = UNSIGNED_ZERO;
                }
                
            }
            buffers.Enqueue[VALIDATION_IDX_OF_TRACKING_BUFFER] = VALIDATION_CURSOR_BUFFER_MARK;
            buffers.Dequeue[VALIDATION_IDX_OF_TRACKING_BUFFER] = VALIDATION_CURSOR_BUFFER_MARK;
            return true;
        }

        static constexpr bool ValidateInitialCursorBuffers(
            CursorBuffers& enqueue_dequeue_buffers,
            const TrackingBufferOfAPC& schema_buffer
        ) noexcept
        {
            auto SetZeroMark = [&]()
            {
                enqueue_dequeue_buffers.Enqueue[VALIDATION_IDX_OF_TRACKING_BUFFER] = UNSIGNED_ZERO;
                enqueue_dequeue_buffers.Dequeue[VALIDATION_IDX_OF_TRACKING_BUFFER] = UNSIGNED_ZERO;
            };

            if (
                !HasSchemaBufferValidationMark(schema_buffer)
            )
            {
                SetZeroMark();
                return false;
            }

            for (uint8_t i = 0; i < APCDataStructure::CountOfMacroColumn(); i++)
            {
                SchemaDefinition::RegionSchemaRecord schema{};
                schema.ParentColumn = GetMacroColumnFromBufferIdx(i);
                if (!SchemaDefinition::LayoutSchemaFromPackedCell(
                    schema,
                    schema_buffer[i]
                ))
                {
                    SetZeroMark();
                    return false;
                }

                const bool is_active_mpmcq = 
                    schema.Protocol == SchemaDefinition::SchemaProtocols::MPMC_FIXED_RECORD_QUEUE &&
                    !SchemaDefinition::HasSchemaFlag(schema.Flags, SchemaDefinition::SchemaFlags::REGION_DISABLED);
                if (is_active_mpmcq)
                {
                    if (
                        enqueue_dequeue_buffers.Enqueue[i] != UNSIGNED_ZERO ||
                        enqueue_dequeue_buffers.Dequeue[i] != UNSIGNED_ZERO
                    )
                    {
                        SetZeroMark();
                        return false;
                    }
                }
                else if (
                    !APCDataStructure::IsValidFabricUnit(enqueue_dequeue_buffers.Enqueue[i]) ||
                    !APCDataStructure::IsValidFabricUnit(enqueue_dequeue_buffers.Dequeue[i])
                )
                {
                    SetZeroMark();
                    return false;
                }
            }
            enqueue_dequeue_buffers.Enqueue[VALIDATION_IDX_OF_TRACKING_BUFFER] = VALIDATION_CURSOR_BUFFER_MARK;
            enqueue_dequeue_buffers.Dequeue[VALIDATION_IDX_OF_TRACKING_BUFFER] = VALIDATION_CURSOR_BUFFER_MARK;
            return true;
        }
    };

}