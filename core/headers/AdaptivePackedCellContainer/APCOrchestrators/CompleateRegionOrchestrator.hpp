#pragma once
#include "SchemaOrchestratorForRegion.hpp"

namespace PredictedAdaptedEncoding
{
    
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

            for (uint8_t i = 0; i < RegionCursorIndexOrchestrator::TrackedAPCNodeLen(); i++)
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