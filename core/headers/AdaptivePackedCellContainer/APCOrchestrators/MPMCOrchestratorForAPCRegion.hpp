#pragma once
#include "LayoutBoundsOrchestrator.hpp"

namespace PredictedAdaptedEncoding
{
    struct MPMCOrchestratorForAPCRegion
    {
        enum class RegionConcurrencyProtocol : uint8_t
        {
            PRIVATE_REGION = 0,
            IMMUTABLE_SNAPSHOT = 1,
            ATOMIC_WORD_ARRAY = 2,
            MPMC_FIXED_RECORD_QUEUE = 3,
            DOUBLE_BUFFERED = 4,
            UNASSIGNED_UNUSED_NANNULL = 5
        };

        enum class DataTypeOfMacroColumn : uint8_t
        {
            UINT8_T = 0,
            UINT16_T = 1,
            UINT32_T = 2,
            UINT64_T = 3,
            INT8_T = 4,
            INT16_T = 5,
            INT32_T = 6,
            INT64_T = 7,
            FLOAT16_T = 8,
            FLOAT32_T = 9,
            FLOAT64_T = 10,
            CHAR = 11,
            UNASSIGNED_UNUSED_NANNULL = 12
        };

        //LEN
        static constexpr uint8_t WORDS_PER_RECORD_LEN = LEN_OF_BYTE_IN_BITS * sizeof(uint16_t);
        static constexpr uint8_t RECORD_WORDS_LEN = LEN_OF_BYTE_IN_BITS * sizeof(uint16_t);
        static constexpr uint8_t PROTOCOL_LEN = LEN_OF_BYTE_IN_BITS * sizeof(RegionConcurrencyProtocol);
        static constexpr uint8_t DTYPE_LEN = LEN_OF_BYTE_IN_BITS * sizeof(uint8_t);
        static constexpr uint8_t VERSION_LEN = LEN_OF_BYTE_IN_BITS * sizeof(uint8_t);
        static constexpr uint8_t FLAGS_LEN = LEN_OF_BYTE_IN_BITS * sizeof(uint8_t);

        //SHIFT
        static constexpr uint8_t FLAGS_SHIFT = (LEN_OF_BYTE_IN_BITS * sizeof(uint64_t)) - FLAGS_LEN;
        static constexpr uint8_t VERSION_SHIFT = FLAGS_SHIFT - VERSION_LEN;
        static constexpr uint8_t DTYPE_SHIFT = VERSION_SHIFT - DTYPE_LEN;
        static constexpr uint8_t PROTOCOL_SHIFT = DTYPE_SHIFT - PROTOCOL_LEN;
        static constexpr uint8_t RECORD_WORDS_SHIFT = PROTOCOL_SHIFT - RECORD_WORDS_LEN;
        static constexpr uint8_t WORDS_PER_RECORD_SHIFT = RECORD_WORDS_SHIFT - WORDS_PER_RECORD_LEN;

        struct RegionSchemaRecord
        {
            uint16_t PayloadWordsPerRecord = UNSIGNED_ZERO;
            uint16_t RecordWords = UNSIGNED_ZERO;
            RegionConcurrencyProtocol Protocol = RegionConcurrencyProtocol::UNASSIGNED_UNUSED_NANNULL;
            DataTypeOfMacroColumn Dtype = DataTypeOfMacroColumn::UNASSIGNED_UNUSED_NANNULL;
            uint8_t Version = UNSIGNED_ZERO;
            uint8_t Flags = UNSIGNED_ZERO;
        };


        static constexpr bool IsValidRegionScheme(const RegionSchemaRecord& desired_scheme) noexcept
        {
            if (
                desired_scheme.PayloadWordsPerRecord > UNSIGNED_ZERO &&
                desired_scheme.RecordWords > UNSIGNED_ZERO &&
                desired_scheme.Protocol != RegionConcurrencyProtocol::UNASSIGNED_UNUSED_NANNULL &&
                desired_scheme.Dtype != DataTypeOfMacroColumn::UNASSIGNED_UNUSED_NANNULL;
                desired_scheme.Version > UNSIGNED_ZERO
            )
            {
                return true;
            }
            return false;
        }


        static constexpr uint64_t PackRegionScheme(const RegionSchemaRecord& desired_scheme)
        {
            if (!IsValidRegionScheme(desired_scheme))
            {
                return FABRIC_CELL_SENTINAL;
            }

            return(
                static_cast<uint64_t>(desired_scheme.PayloadWordsPerRecord) << WORDS_PER_RECORD_SHIFT |
                static_cast<uint64_t>(desired_scheme.RecordWords) << RECORD_WORDS_SHIFT |
                static_cast<uint64_t>(desired_scheme.Protocol) << PROTOCOL_SHIFT |
                static_cast<uint64_t>(desired_scheme.Dtype) << DTYPE_SHIFT |
                static_cast<uint64_t>(desired_scheme.Version) << VERSION_SHIFT |
                static_cast<uint64_t>(desired_scheme.Flags) << FLAGS_SHIFT
            );       
        }


        static constexpr bool RegionSchemeFromPackedRegion(
            RegionSchemaRecord& return_scheme,
            uint64_t packed_scheme
        ) noexcept
        {
            if (!APCDataStructure::IsThsisIndexValidForFabric(packed_scheme))
            {
                return_scheme = RegionSchemaRecord{};
                return false;
            }

            return_scheme.PayloadWordsPerRecord = static_cast<uint16_t>((packed_scheme >> WORDS_PER_RECORD_SHIFT) & MaskLeftOverBitsUntil64(WORDS_PER_RECORD_LEN));
            return_scheme.RecordWords = static_cast<uint16_t>((packed_scheme >> RECORD_WORDS_SHIFT) & MaskLeftOverBitsUntil64(RECORD_WORDS_LEN));
            return_scheme.Protocol = static_cast<RegionConcurrencyProtocol>((packed_scheme >> PROTOCOL_SHIFT) & MaskLeftOverBitsUntil64(PROTOCOL_LEN));
            return_scheme.Dtype = static_cast<DataTypeOfMacroColumn>((packed_scheme >> DTYPE_SHIFT) & MaskLeftOverBitsUntil64(DTYPE_LEN));
            return_scheme.Version = static_cast<uint8_t>((packed_scheme >> VERSION_SHIFT) & MaskLeftOverBitsUntil64(VERSION_LEN));
            return_scheme.Flags = static_cast<uint8_t>((packed_scheme >> FLAGS_SHIFT) & MaskLeftOverBitsUntil64(FLAGS_LEN));

            if (!IsValidRegionScheme(return_scheme))
            {
                return_scheme = RegionSchemaRecord{};
                return false;
            }
            return true;
        }

    };

    struct RegionCursorOrchestrator : public TrackingBufferConf
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
    };
    

}