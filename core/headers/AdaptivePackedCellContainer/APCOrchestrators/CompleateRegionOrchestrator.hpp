#pragma once
#include "SchemaOrchestratorForRegion.hpp"

namespace PredictedAdaptedEncoding
{

    struct RegionOrchestrator : SchemDefinition
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
                    (return_schema.RequiredTypedElementsPerRecord % 2u) != UNSIGNED_ZERO
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
            
            default:
                schema.IsValidSchema = false;
                return false;
            }
            
            
        }

    };

    struct CompleateRegionOrchestrator : public TrackingBufferConf
    {
        static constexpr uint64_t VALIDATION_CURSOR_BUFFER_MARK = 22222u;

        struct CursorBuffers
        {
            TrackingBufferOfAPC Enqueue{};
            TrackingBufferOfAPC Dequeue{};
        };

        static constexpr void BuildInitialCursorBuffers(CursorBuffers& buffers) noexcept
        {
            BuildNullTrackingBuffer(buffers.Enqueue);
            BuildNullTrackingBuffer(buffers.Dequeue);

            for (uint8_t i = 0; i < MacroColumnConf::TrackedAPCNodeLen(); i++)
            {
                buffers.Enqueue[i] = UNSIGNED_ZERO;
                buffers.Dequeue[i] = UNSIGNED_ZERO;
            }
            buffers.Enqueue[VALIDATION_IDX_OF_TRACKING_BUFFER] = VALIDATION_CURSOR_BUFFER_MARK;
            buffers.Dequeue[VALIDATION_IDX_OF_TRACKING_BUFFER] = VALIDATION_CURSOR_BUFFER_MARK;
        }

        static constexpr bool InsertASchemaInBuffer(
            TrackingBufferOfAPC& buffer_address,
            RegionOrchestrator::RegionSchemaRecord& schema_record,
            MacroColumnOfAPC desired_column
        ) noexcept
        {

            const std::optional<uint8_t> buffer_idx = GetBufferIdxFromMacroColumn(desired_column);
            if (!buffer_idx.has_value())
            {
                return false;
            }
                       
            uint64_t packed_schema = RegionOrchestrator::PackRegionScheme(schema_record);
            if (!APCDataStructure::IsThsisIndexValidForFabric(packed_schema))
            {
                return false;
            }

            buffer_address[*buffer_idx] = packed_schema; 

            return true;
        }

        static constexpr bool BuildInitialSchema() noexcept;

        static constexpr bool ValidateInitialSchema() noexcept;
    };

}