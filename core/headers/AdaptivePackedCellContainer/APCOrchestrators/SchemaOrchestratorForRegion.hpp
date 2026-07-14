#pragma once
#include "LayoutBoundsOrchestrator.hpp"

namespace PredictedAdaptedEncoding
{

    struct SchemaOrchestrator 
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

        enum class SchemaFlags : uint8_t
        {
            NONE = 0u,
            REQUIRED_POW_OF_TWO = 1u << 0u,
            ALLOW_QUICENT_SCHEMA_MUTATION = 1u << 1u ,
            ALLOW_TRAILING_PAGING = 1u << 2u,
            HAS_PER_SLOT_SEQUENSE = 1u << 3u,
            REGION_DISABLED = 1u << 4u,
            UNASSIGNED_UNUSED_NANNULL = 1u << 3u
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
            SchemaFlags Flags = SchemaFlags::UNASSIGNED_UNUSED_NANNULL;
        };

        struct InitialRegionalDtypeConf 
        {
            DataTypeOfMacroColumn FEEDFORWARD_MESSAGE  = DataTypeOfMacroColumn::UINT32_T;
            DataTypeOfMacroColumn FEEDBACKWARD_MESSAGE = DataTypeOfMacroColumn::INT32_T;
            DataTypeOfMacroColumn LATERAL_MESAGE = DataTypeOfMacroColumn::FLOAT32_T;
            DataTypeOfMacroColumn STATE_SLOT = DataTypeOfMacroColumn::UINT8_T;
            DataTypeOfMacroColumn ERROR_SLOT = DataTypeOfMacroColumn::UINT8_T;
            DataTypeOfMacroColumn WEIGHTLESS_LOOKUP = DataTypeOfMacroColumn::UINT8_T;
            DataTypeOfMacroColumn WEIGHT_SLOT = DataTypeOfMacroColumn::UINT8_T;
            DataTypeOfMacroColumn AUX_SLOT = DataTypeOfMacroColumn::UINT8_T;
            DataTypeOfMacroColumn HETEROGENOUS_PTR = DataTypeOfMacroColumn::UINT64_T;
            DataTypeOfMacroColumn FREE_SLOT = DataTypeOfMacroColumn::UINT64_T;
        }; 

        struct InitialConcurrencyProtocol
        {
            RegionConcurrencyProtocol FEEDFORWARD_MESSAGE  = RegionConcurrencyProtocol::MPMC_FIXED_RECORD_QUEUE;
            RegionConcurrencyProtocol FEEDBACKWARD_MESSAGE = RegionConcurrencyProtocol::MPMC_FIXED_RECORD_QUEUE;
            RegionConcurrencyProtocol LATERAL_MESAGE = RegionConcurrencyProtocol::MPMC_FIXED_RECORD_QUEUE;
            RegionConcurrencyProtocol STATE_SLOT = RegionConcurrencyProtocol::DOUBLE_BUFFERED;
            RegionConcurrencyProtocol ERROR_SLOT = RegionConcurrencyProtocol::DOUBLE_BUFFERED;
            RegionConcurrencyProtocol WEIGHTLESS_LOOKUP = RegionConcurrencyProtocol::DOUBLE_BUFFERED;
            RegionConcurrencyProtocol WEIGHT_SLOT = RegionConcurrencyProtocol::DOUBLE_BUFFERED;
            RegionConcurrencyProtocol AUX_SLOT = RegionConcurrencyProtocol::DOUBLE_BUFFERED;
            RegionConcurrencyProtocol HETEROGENOUS_PTR = RegionConcurrencyProtocol::PRIVATE_REGION;
            RegionConcurrencyProtocol FREE_SLOT = RegionConcurrencyProtocol::PRIVATE_REGION;
        };

    };
    
    struct SchemaValidator : public SchemaOrchestrator
    {
        static constexpr bool IsValidRegionScheme(const RegionSchemaRecord& desired_scheme) noexcept
        {
            if (
                desired_scheme.PayloadWordsPerRecord == UNSIGNED_ZERO ||
                desired_scheme.RecordWords == UNSIGNED_ZERO ||
                !IsKnownProtocol(desired_scheme.Protocol) ||
                !IsKnownDataType(desired_scheme.Dtype) ||
                desired_scheme.Version == UNSIGNED_ZERO
            )
            {
                return false;
            }
            if (IsMPMCQueue(desired_scheme))
            {
                return desired_scheme.RecordWords == static_cast<uint16_t>(desired_scheme.PayloadWordsPerRecord + 1u);
            }

            return desired_scheme.RecordWords >= desired_scheme.PayloadWordsPerRecord;
        }

        static constexpr bool IsKnownProtocol(RegionConcurrencyProtocol protocol) noexcept
        {
            return protocol >= RegionConcurrencyProtocol::PRIVATE_REGION &&
                protocol < RegionConcurrencyProtocol::UNASSIGNED_UNUSED_NANNULL;
        }

        static constexpr bool IsKnownDataType(DataTypeOfMacroColumn data_type) noexcept
        {
            return data_type >= DataTypeOfMacroColumn::UINT8_T &&
                data_type < DataTypeOfMacroColumn::UNASSIGNED_UNUSED_NANNULL;
        }

        static constexpr bool IsMPMCQueue(const RegionSchemaRecord& provided_schema) noexcept
        {
            return provided_schema.Protocol == RegionConcurrencyProtocol::MPMC_FIXED_RECORD_QUEUE;
        }

        static constexpr bool HasSchemaFlag(SchemaFlags current_flag, SchemaFlags desired_bit) noexcept
        {
            return (
                static_cast<uint8_t>(current_flag) & static_cast<uint8_t>(desired_bit)
            ) != UNSIGNED_ZERO;
        }

        

    };
    

    struct RegionOrchestrator : public SchemaValidator
    {


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
            return_scheme.Flags = static_cast<SchemaFlags>((packed_scheme >> FLAGS_SHIFT) & MaskLeftOverBitsUntil64(FLAGS_LEN));

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

    namespace Schema
    {
        static constexpr SchemaOrchestrator::SchemaFlags operator|(SchemaOrchestrator::SchemaFlags lhs, SchemaOrchestrator::SchemaFlags rhs) noexcept
        {
            return static_cast<SchemaOrchestrator::SchemaFlags>(
                static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs)
            );
        }
    } // namespace Schema
    
    
}