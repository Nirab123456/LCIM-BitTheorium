#pragma once
#include "LayoutBoundsOrchestrator.hpp"

namespace PredictedAdaptedEncoding
{

    struct SchemaOrchestrator 
    {
        enum class SchemaProtocols : uint8_t
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
        static constexpr uint8_t PROTOCOL_LEN = LEN_OF_BYTE_IN_BITS * sizeof(SchemaProtocols);
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
            SchemaProtocols Protocol = SchemaProtocols::UNASSIGNED_UNUSED_NANNULL;
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
            SchemaProtocols FEEDFORWARD_MESSAGE  = SchemaProtocols::MPMC_FIXED_RECORD_QUEUE;
            SchemaProtocols FEEDBACKWARD_MESSAGE = SchemaProtocols::MPMC_FIXED_RECORD_QUEUE;
            SchemaProtocols LATERAL_MESAGE = SchemaProtocols::MPMC_FIXED_RECORD_QUEUE;
            SchemaProtocols STATE_SLOT = SchemaProtocols::DOUBLE_BUFFERED;
            SchemaProtocols ERROR_SLOT = SchemaProtocols::DOUBLE_BUFFERED;
            SchemaProtocols WEIGHTLESS_LOOKUP = SchemaProtocols::DOUBLE_BUFFERED;
            SchemaProtocols WEIGHT_SLOT = SchemaProtocols::DOUBLE_BUFFERED;
            SchemaProtocols AUX_SLOT = SchemaProtocols::DOUBLE_BUFFERED;
            SchemaProtocols HETEROGENOUS_PTR = SchemaProtocols::PRIVATE_REGION;
            SchemaProtocols FREE_SLOT = SchemaProtocols::PRIVATE_REGION;
        };

    };
    
    struct SchemaValidator : public SchemaOrchestrator
    {
        static constexpr bool IsValidRegionScheme(const RegionSchemaRecord& desired_scheme) noexcept
        {
            if (
                !IsKnownProtocol(desired_scheme.Protocol) ||
                !IsKnownDataType(desired_scheme.Dtype) ||
                desired_scheme.Version == UNSIGNED_ZERO
            )
            {
                return false;
            }
            if (HasSchemaFlag(desired_scheme.Flags, SchemaFlags::REGION_DISABLED))
            {
                return desired_scheme.PayloadWordsPerRecord == UNSIGNED_ZERO &&
                    desired_scheme.RecordWords == UNSIGNED_ZERO;
            }

            if (
                desired_scheme.PayloadWordsPerRecord == UNSIGNED_ZERO ||
                desired_scheme.RecordWords == UNSIGNED_ZERO
            )
            {
                return false;
            }

            switch (desired_scheme.Protocol)
            {
            case SchemaProtocols::MPMC_FIXED_RECORD_QUEUE:
                return desired_scheme.RecordWords == static_cast<uint16_t>(desired_scheme.PayloadWordsPerRecord + 1u) &&
                    HasSchemaFlag(desired_scheme.Flags, SchemaFlags::REQUIRED_POW_OF_TWO) &&
                    HasSchemaFlag(desired_scheme.Flags, SchemaFlags::HAS_PER_SLOT_SEQUENSE);
            
            case SchemaProtocols::ATOMIC_WORD_ARRAY:
                return desired_scheme.PayloadWordsPerRecord == 1u &&
                    desired_scheme.RecordWords == 1u;

            case SchemaProtocols::PRIVATE_REGION:
            case SchemaProtocols::IMMUTABLE_SNAPSHOT:
            case SchemaProtocols::DOUBLE_BUFFERED:
                return desired_scheme.RecordWords == desired_scheme.PayloadWordsPerRecord;
            
            default:
                return false;
            }
            
            
            if (IsMPMCQueue(desired_scheme))
            {
                return desired_scheme.RecordWords == static_cast<uint16_t>(desired_scheme.PayloadWordsPerRecord + 1u);
            }

            return desired_scheme.RecordWords >= desired_scheme.PayloadWordsPerRecord;
        }

        static constexpr bool IsKnownProtocol(SchemaProtocols protocol) noexcept
        {
            return protocol >= SchemaProtocols::PRIVATE_REGION &&
                protocol < SchemaProtocols::UNASSIGNED_UNUSED_NANNULL;
        }

        static constexpr bool IsKnownDataType(DataTypeOfMacroColumn data_type) noexcept
        {
            return data_type >= DataTypeOfMacroColumn::UINT8_T &&
                data_type < DataTypeOfMacroColumn::UNASSIGNED_UNUSED_NANNULL;
        }

        static constexpr bool IsMPMCQueue(const RegionSchemaRecord& provided_schema) noexcept
        {
            return provided_schema.Protocol == SchemaProtocols::MPMC_FIXED_RECORD_QUEUE;
        }

        static constexpr bool HasSchemaFlag(SchemaFlags current_flag, SchemaFlags desired_bit) noexcept
        {
            return (
                static_cast<uint8_t>(current_flag) & static_cast<uint8_t>(desired_bit)
            ) != UNSIGNED_ZERO;
        }


        static constexpr bool IsKnownSchemaFlags(SchemaFlags flags) noexcept
        {
            constexpr uint8_t KNOWN_FLAGS = 
                static_cast<uint8_t>(SchemaFlags::REQUIRED_POW_OF_TWO) |
                static_cast<uint8_t>(SchemaFlags::ALLOW_QUICENT_SCHEMA_MUTATION) |
                static_cast<uint8_t>(SchemaFlags::ALLOW_TRAILING_PAGING) |
                static_cast<uint8_t>(SchemaFlags::HAS_PER_SLOT_SEQUENSE) |
                static_cast<uint8_t>(SchemaFlags::REGION_DISABLED);
            
            const uint8_t raw = static_cast<uint8_t>(flags);

            return flags != SchemaFlags::UNASSIGNED_UNUSED_NANNULL &&
                (raw & static_cast<uint8_t>(~KNOWN_FLAGS)) == UNSIGNED_ZERO;
                
        }

        

    };

    static constexpr SchemaOrchestrator::SchemaFlags operator|(SchemaOrchestrator::SchemaFlags lhs, SchemaOrchestrator::SchemaFlags rhs) noexcept
    {
        return static_cast<SchemaOrchestrator::SchemaFlags>(
            static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs)
        );
    }

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
            return_scheme.Protocol = static_cast<SchemaProtocols>((packed_scheme >> PROTOCOL_SHIFT) & MaskLeftOverBitsUntil64(PROTOCOL_LEN));
            return_scheme.Dtype = static_cast<DataTypeOfMacroColumn>((packed_scheme >> DTYPE_SHIFT) & MaskLeftOverBitsUntil64(DTYPE_LEN));
            return_scheme.Version = static_cast<uint8_t>((packed_scheme >> VERSION_SHIFT) & MaskLeftOverBitsUntil64(VERSION_LEN));
            return_scheme.Flags = static_cast<SchemaFlags>((packed_scheme >> FLAGS_SHIFT) & MaskLeftOverBitsUntil64(FLAGS_LEN));

        }

        static constexpr SchemaProtocols GetProtocolForColumn(
            const InitialConcurrencyProtocol& conc_conf,
            MacroColumnOfAPC column
        ) noexcept
        {
            switch (column)
            {
            case MacroColumnOfAPC::FEEDFORWARD_MESSAGE: return conc_conf.FEEDFORWARD_MESSAGE;
            case MacroColumnOfAPC::FEEDBACKWARD_MESSAGE: return conc_conf.FEEDBACKWARD_MESSAGE;
            case MacroColumnOfAPC::LATERAL_MESAGE: return conc_conf.LATERAL_MESAGE;
            case MacroColumnOfAPC::STATE_SLOT: return conc_conf.STATE_SLOT;
            case MacroColumnOfAPC::ERROR_SLOT: return conc_conf.ERROR_SLOT;
            case MacroColumnOfAPC::WEIGHTLESS_LOOKUP: return conc_conf.WEIGHTLESS_LOOKUP;
            case MacroColumnOfAPC::WEIGHT_SLOT: return conc_conf.WEIGHT_SLOT;
            case MacroColumnOfAPC::AUX_SLOT: return conc_conf.AUX_SLOT;
            case MacroColumnOfAPC::HETEROGENOUS_PTR: return conc_conf.HETEROGENOUS_PTR;
            case MacroColumnOfAPC::FREE_SLOT: return conc_conf.FREE_SLOT;
            default: return SchemaProtocols::UNASSIGNED_UNUSED_NANNULL;
            }
        }

        static constexpr DataTypeOfMacroColumn GetDataTypeForColumn(
            const InitialRegionalDtypeConf& conc_conf,
            MacroColumnOfAPC column
        ) noexcept
        {
            switch (column)
            {
            case MacroColumnOfAPC::FEEDFORWARD_MESSAGE: return conc_conf.FEEDFORWARD_MESSAGE;
            case MacroColumnOfAPC::FEEDBACKWARD_MESSAGE: return conc_conf.FEEDBACKWARD_MESSAGE;
            case MacroColumnOfAPC::LATERAL_MESAGE: return conc_conf.LATERAL_MESAGE;
            case MacroColumnOfAPC::STATE_SLOT: return conc_conf.STATE_SLOT;
            case MacroColumnOfAPC::ERROR_SLOT: return conc_conf.ERROR_SLOT;
            case MacroColumnOfAPC::WEIGHTLESS_LOOKUP: return conc_conf.WEIGHTLESS_LOOKUP;
            case MacroColumnOfAPC::WEIGHT_SLOT: return conc_conf.WEIGHT_SLOT;
            case MacroColumnOfAPC::AUX_SLOT: return conc_conf.AUX_SLOT;
            case MacroColumnOfAPC::HETEROGENOUS_PTR: return conc_conf.HETEROGENOUS_PTR;
            case MacroColumnOfAPC::FREE_SLOT: return conc_conf.FREE_SLOT;
            default: return DataTypeOfMacroColumn::UNASSIGNED_UNUSED_NANNULL;
            }
        }

        static constexpr bool MakeInitialRegionSchema(
            RegionSchemaRecord& return_schema,
            uint16_t region_span,
            MacroColumnOfAPC default_setter = MacroColumnOfAPC::NULLNAN
        ) noexcept
        {
            if (default_setter == MacroColumnOfAPC::NULLNAN)
            {
                if (
                    !IsKnownDataType(return_schema.Dtype) ||
                    !IsKnownProtocol(return_schema.Protocol) ||
                    !APCDataStructure::ThisVersionValid(return_schema.Version)
                )
                {
                    return false;
                }
            }
            const InitialConcurrencyProtocol init_protocol = InitialConcurrencyProtocol{};
            const InitialRegionalDtypeConf init_dtype = InitialRegionalDtypeConf{};

            return_schema.Dtype = IsKnownDataType(return_schema.Dtype) ? return_schema.Dtype : GetDataTypeForColumn(init_dtype, default_setter);
            return_schema.Protocol = IsKnownProtocol(return_schema.Protocol) ? return_schema.Protocol : GetProtocolForColumn(init_protocol, default_setter);
            return_schema.Version = APCDataStructure::ThisVersionValid(return_schema.Version) ? return_schema.Version : APCDataStructure::BRANCH_VERSION;
            return_schema.Flags = SchemaFlags::NONE;

            if (region_span == UNSIGNED_ZERO)
            {
                return_schema.Flags = SchemaFlags::REGION_DISABLED;
                return true;
            }

            switch (return_schema.Protocol)
            {
            case SchemaProtocols::MPMC_FIXED_RECORD_QUEUE:
                return_schema.PayloadWordsPerRecord = 1u;
                return_schema.RecordWords = 2u;
                return_schema.Flags = 
                    SchemaFlags::REQUIRED_POW_OF_TWO |
                    SchemaFlags::ALLOW_TRAILING_PAGING |
                    SchemaFlags::HAS_PER_SLOT_SEQUENSE;
                return true;

            case SchemaProtocols::DOUBLE_BUFFERED:
                return_schema.PayloadWordsPerRecord = static_cast<uint16_t>(region_span / 2u);
                return_schema.RecordWords = return_schema.PayloadWordsPerRecord;
                if ((region_span % 2u) != UNSIGNED_ZERO)
                {
                    return_schema.Flags = SchemaFlags::ALLOW_TRAILING_PAGING;
                }
                return true;

            case SchemaProtocols::ATOMIC_WORD_ARRAY:
                return_schema.PayloadWordsPerRecord = 1u;
                return_schema.RecordWords = 1u;
                break;
            
            case SchemaProtocols::PRIVATE_REGION:
            case SchemaProtocols::IMMUTABLE_SNAPSHOT:
                return_schema.PayloadWordsPerRecord = region_span;
                return_schema.RecordWords = region_span;
                return true;
                
            default:
                return false;
            }
            
        }

    };
    
    
}