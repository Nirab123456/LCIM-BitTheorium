#pragma once
#include "LayoutBoundsOrchestrator.hpp"

namespace BidirectionalInMemGraph
{

    struct SchemaOrchestrator 
    {
        enum class SchemaProtocols : uint8_t
        {
            PRIVATE_REGION = 0,
            IMMUTABLE_SNAPSHOT = 1,
            ATOMIC_WORD_ARRAY = 2,
            MPMC_FIXED_RECORD_QUEUE = 3,
            DOUBLE_BUFFERED = 4
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
            CHAR = 11
        };

        enum class SchemaFlags : uint8_t
        {
            NONE = 0u,
            REQUIRED_POW_OF_TWO = 1u << 0u,
            ALLOW_QUICENT_SCHEMA_MUTATION = 1u << 1u ,
            ALLOW_TRAILING_PADDING = 1u << 2u,
            HAS_PER_SLOT_SEQUENSE = 1u << 3u,
            REGION_DISABLED = 1u << 4u,
            UNASSIGNED_UNUSED_NANNULL = UINT8_MAX
        };

        //LEN
        static constexpr uint8_t WORDS_PER_RECORD_LEN = LEN_OF_BYTE_IN_BITS * sizeof(uint32_t);
        static constexpr uint8_t PROTOCOL_LEN = LEN_OF_BYTE_IN_BITS * sizeof(SchemaProtocols);
        static constexpr uint8_t DTYPE_LEN = LEN_OF_BYTE_IN_BITS * sizeof(uint8_t);
        static constexpr uint8_t VERSION_LEN = LEN_OF_BYTE_IN_BITS * sizeof(uint8_t);
        static constexpr uint8_t FLAGS_LEN = LEN_OF_BYTE_IN_BITS * sizeof(uint8_t);

        //SHIFT
        static constexpr uint8_t FLAGS_SHIFT = (LEN_OF_BYTE_IN_BITS * sizeof(uint64_t)) - FLAGS_LEN;
        static constexpr uint8_t VERSION_SHIFT = FLAGS_SHIFT - VERSION_LEN;
        static constexpr uint8_t DTYPE_SHIFT = VERSION_SHIFT - DTYPE_LEN;
        static constexpr uint8_t PROTOCOL_SHIFT = DTYPE_SHIFT - PROTOCOL_LEN;
        static constexpr uint8_t WORDS_PER_RECORD_SHIFT = PROTOCOL_SHIFT - WORDS_PER_RECORD_LEN;

        struct RegionSchemaRecord
        {
            uint32_t RequiredTypedElementsPerRecord = UNSIGNED_ZERO;
            SchemaProtocols Protocol{};
            DataTypeOfMacroColumn Dtype{};
            uint8_t Version = UNSIGNED_ZERO;
            SchemaFlags Flags = SchemaFlags::UNASSIGNED_UNUSED_NANNULL;
            MacroColumnOfAPC ParentColumn{};
            bool IsValidSchema = false;
        };

        struct InitialRegionalDtypeConf 
        {
            DataTypeOfMacroColumn FEEDFORWARD_MESSAGE  = DataTypeOfMacroColumn::FLOAT32_T;
            DataTypeOfMacroColumn FEEDBACKWARD_MESSAGE = DataTypeOfMacroColumn::FLOAT32_T;
            DataTypeOfMacroColumn LATERAL_MESAGE = DataTypeOfMacroColumn::FLOAT32_T;
            DataTypeOfMacroColumn STATE_SLOT = DataTypeOfMacroColumn::UINT8_T;
            DataTypeOfMacroColumn ERROR_SLOT = DataTypeOfMacroColumn::FLOAT32_T;
            DataTypeOfMacroColumn WEIGHTLESS_LOOKUP = DataTypeOfMacroColumn::UINT8_T;
            DataTypeOfMacroColumn WEIGHT_SLOT = DataTypeOfMacroColumn::UINT8_T;
            DataTypeOfMacroColumn AUX_SLOT = DataTypeOfMacroColumn::UINT8_T;
            DataTypeOfMacroColumn HETEROGENOUS_PTR = DataTypeOfMacroColumn::UINT64_T;
            DataTypeOfMacroColumn FREE_SLOT = DataTypeOfMacroColumn::UINT64_T;
        }; 

        struct InitialRegionalProtocol
        {
            SchemaProtocols FEEDFORWARD_MESSAGE  = SchemaProtocols::ATOMIC_WORD_ARRAY;
            SchemaProtocols FEEDBACKWARD_MESSAGE = SchemaProtocols::ATOMIC_WORD_ARRAY;
            SchemaProtocols LATERAL_MESAGE = SchemaProtocols::MPMC_FIXED_RECORD_QUEUE;
            SchemaProtocols STATE_SLOT = SchemaProtocols::DOUBLE_BUFFERED;
            SchemaProtocols ERROR_SLOT = SchemaProtocols::PRIVATE_REGION;
            SchemaProtocols WEIGHTLESS_LOOKUP = SchemaProtocols::DOUBLE_BUFFERED;
            SchemaProtocols WEIGHT_SLOT = SchemaProtocols::DOUBLE_BUFFERED;
            SchemaProtocols AUX_SLOT = SchemaProtocols::DOUBLE_BUFFERED;
            SchemaProtocols HETEROGENOUS_PTR = SchemaProtocols::PRIVATE_REGION;
            SchemaProtocols FREE_SLOT = SchemaProtocols::PRIVATE_REGION;
        };

    };
    
    struct SchemaValidator : public SchemaOrchestrator
    {
        static constexpr bool HasEnoughForInitialSchema(const RegionSchemaRecord& schema) noexcept
        {
            if (
                !APCDataStructure::InLimitOfUint8(schema.Version)
            )
            {
                return false;
            }

            return true;
        }

        static constexpr bool SchemaSelfValidation(RegionSchemaRecord& desired_scheme) noexcept
        {
            if (
                !APCDataStructure::InLimitOfUint8(desired_scheme.Version)
            )
            {
                desired_scheme.IsValidSchema = false;
                return false;
            }
            if (HasSchemaFlag(desired_scheme.Flags, SchemaFlags::REGION_DISABLED))
            {
                desired_scheme.IsValidSchema = desired_scheme.RequiredTypedElementsPerRecord == UNSIGNED_ZERO;
                return desired_scheme.IsValidSchema;
            }

            if ( desired_scheme.RequiredTypedElementsPerRecord == UNSIGNED_ZERO)
            {
                desired_scheme.IsValidSchema = false;
                return false;
            }

            switch (desired_scheme.Protocol)
            {
            case SchemaProtocols::MPMC_FIXED_RECORD_QUEUE:
                desired_scheme.IsValidSchema = 
                    HasSchemaFlag(desired_scheme.Flags, SchemaFlags::REQUIRED_POW_OF_TWO) &&
                    HasSchemaFlag(desired_scheme.Flags, SchemaFlags::HAS_PER_SLOT_SEQUENSE);
                return desired_scheme.IsValidSchema;
            
            case SchemaProtocols::ATOMIC_WORD_ARRAY:
                desired_scheme.IsValidSchema = desired_scheme.RequiredTypedElementsPerRecord != UNSIGNED_ZERO;
                return desired_scheme.IsValidSchema;

            case SchemaProtocols::PRIVATE_REGION:
            case SchemaProtocols::IMMUTABLE_SNAPSHOT:
            case SchemaProtocols::DOUBLE_BUFFERED:
                desired_scheme.IsValidSchema = true;
                return true;
            
            default:
                desired_scheme.IsValidSchema = false;
                return false;
            }                                                                                                                                                                                                                                                                                                                                                                           
        }

        static constexpr bool IsSchemaValidated(const RegionSchemaRecord& schema) noexcept
        {
            if (schema.IsValidSchema)
            {
                return true;
            }
            return false;
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
                static_cast<uint8_t>(SchemaFlags::ALLOW_TRAILING_PADDING) |
                static_cast<uint8_t>(SchemaFlags::HAS_PER_SLOT_SEQUENSE) |
                static_cast<uint8_t>(SchemaFlags::REGION_DISABLED);
            
            const uint8_t raw = static_cast<uint8_t>(flags);

            return flags != SchemaFlags::UNASSIGNED_UNUSED_NANNULL &&
                (raw & static_cast<uint8_t>(~KNOWN_FLAGS)) == UNSIGNED_ZERO;
                
        }

        static constexpr bool IsValuePowOfTwoU32(uint32_t value) noexcept
        {
            return value >= 2u && ((value & (value - 1u)) == UNSIGNED_ZERO);
        }


        template<class DType>
        static constexpr std::optional<DataTypeOfMacroColumn> CppTypeToRegionDType() noexcept
        {
            using U = std::remove_cv_t<DType>;

            if constexpr (std::is_same_v<U, uint8_t>)
                return DataTypeOfMacroColumn::UINT8_T;

            else if constexpr (std::is_same_v<U, uint16_t>)
                return DataTypeOfMacroColumn::UINT16_T;

            else if constexpr (std::is_same_v<U, uint32_t>)
                return DataTypeOfMacroColumn::UINT32_T;

            else if constexpr (std::is_same_v<U, uint64_t>)
                return DataTypeOfMacroColumn::UINT64_T;

            else if constexpr (std::is_same_v<U, int8_t>)
                return DataTypeOfMacroColumn::INT8_T;

            else if constexpr (std::is_same_v<U, int16_t>)
                return DataTypeOfMacroColumn::INT16_T;

            else if constexpr (std::is_same_v<U, int32_t>)
                return DataTypeOfMacroColumn::INT32_T;

            else if constexpr (std::is_same_v<U, int64_t>)
                return DataTypeOfMacroColumn::INT64_T;

            else if constexpr (std::is_same_v<U, float>)
                return DataTypeOfMacroColumn::FLOAT32_T;

            else if constexpr (std::is_same_v<U, double>)
                return DataTypeOfMacroColumn::FLOAT64_T;

            else if constexpr (std::is_same_v<U, char>)
                return DataTypeOfMacroColumn::CHAR;

            else
                return std::nullopt;
        }

    };

    static constexpr SchemaOrchestrator::SchemaFlags operator|(SchemaOrchestrator::SchemaFlags lhs, SchemaOrchestrator::SchemaFlags rhs) noexcept
    {
        return static_cast<SchemaOrchestrator::SchemaFlags>(
            static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs)
        );
    }

    struct MPMCQOrchestrator : public SchemaValidator
    {
        static constexpr uint8_t FIXED_ADDITIONAL_UIT_FRO_MPMCQ = 1;

        static constexpr std::optional<uint8_t> CountOfTypedWordIn64Bit(DataTypeOfMacroColumn data_type) noexcept
        {
            switch (data_type)
            {
            case DataTypeOfMacroColumn::UINT8_T:
            case DataTypeOfMacroColumn::INT8_T:
            case DataTypeOfMacroColumn::CHAR:
                return static_cast<uint8_t>(sizeof(uint64_t) / sizeof(uint8_t));

            case DataTypeOfMacroColumn::UINT16_T:
            case DataTypeOfMacroColumn::INT16_T:
            case DataTypeOfMacroColumn::FLOAT16_T:
                return static_cast<uint8_t>(sizeof(uint64_t) / sizeof(uint16_t));

            case DataTypeOfMacroColumn::UINT32_T:
            case DataTypeOfMacroColumn::INT32_T:
            case DataTypeOfMacroColumn::FLOAT32_T:
                return static_cast<uint8_t>(sizeof(uint64_t) / sizeof(uint32_t));

            case DataTypeOfMacroColumn::UINT64_T:
            case DataTypeOfMacroColumn::INT64_T:
            case DataTypeOfMacroColumn::FLOAT64_T:
                return static_cast<uint8_t>(sizeof(uint64_t) / sizeof(uint64_t));

            default:
                return std::nullopt;
            }
        }

        static constexpr std::optional<uint32_t> RequiredFbUnitForDesiredPayload(const RegionSchemaRecord& schema) noexcept
        {
            std::optional<uint8_t> count_of_typed_word_in64bit = CountOfTypedWordIn64Bit(schema.Dtype);
            if (
                !schema.IsValidSchema ||
                !count_of_typed_word_in64bit.has_value() ||
                schema.RequiredTypedElementsPerRecord == UNSIGNED_ZERO
            )
            {
                return std::nullopt;
            }

            return UpwordRoundingDivision_(
                schema.RequiredTypedElementsPerRecord,
                count_of_typed_word_in64bit.value()
            );
            
        }

        static constexpr std::optional<uint32_t>CountOf64BitBasedOnTypedProtocol(const RegionSchemaRecord& schema) noexcept
        {
            const std::optional<uint32_t> payload_words = RequiredFbUnitForDesiredPayload(schema);
            if (!payload_words.has_value())
            {
                return std::nullopt;
            }
            return payload_words.value() +
                (schema.Protocol == SchemaProtocols::MPMC_FIXED_RECORD_QUEUE ? FIXED_ADDITIONAL_UIT_FRO_MPMCQ : UNSIGNED_ZERO);
        }

        static constexpr uint32_t FloorPoweOfTwoUnsigned32(uint32_t value) noexcept
        {
            if (value == UNSIGNED_ZERO)
            {
                return UNSIGNED_ZERO;
            }

            uint32_t result = 1u;
            while (result <= value / 2u)
            {
                result <<= 1u;
            }
            return result;
        }

    protected:
        static constexpr uint32_t UpwordRoundingDivision_(
            uint32_t numerator,
            uint32_t denominator
        ) noexcept
        {
            if (denominator == UNSIGNED_ZERO)
            {
                return UNSIGNED_ZERO;
            }

            return numerator / denominator +
                static_cast<uint32_t>(numerator % denominator != UNSIGNED_ZERO);
            
        }
    };


    struct SchemDefinition : public MPMCQOrchestrator
    {
        static constexpr uint64_t PackRegionScheme(RegionSchemaRecord& desired_scheme)
        {
            if (!SchemaSelfValidation(desired_scheme))
            {
                return FABRIC_CELL_SENTINAL;
            }

            return(
                static_cast<uint64_t>(desired_scheme.RequiredTypedElementsPerRecord) << WORDS_PER_RECORD_SHIFT |
                static_cast<uint64_t>(desired_scheme.Protocol) << PROTOCOL_SHIFT |
                static_cast<uint64_t>(desired_scheme.Dtype) << DTYPE_SHIFT |
                static_cast<uint64_t>(desired_scheme.Version) << VERSION_SHIFT |
                static_cast<uint64_t>(desired_scheme.Flags) << FLAGS_SHIFT
            );       
        }


        static constexpr bool LayoutSchemaFromPackedCell(
            RegionSchemaRecord& return_schema,
            uint64_t packed_scheme
        ) noexcept
        {
            if (!APCDataStructure::IsValidFabricUnit(packed_scheme))
            {
                return_schema = RegionSchemaRecord{};
                return false;
            }

            return_schema.RequiredTypedElementsPerRecord = static_cast<uint32_t>((packed_scheme >> WORDS_PER_RECORD_SHIFT) & MaskLeftOverBitsUntil64(WORDS_PER_RECORD_LEN));
            return_schema.Protocol = static_cast<SchemaProtocols>((packed_scheme >> PROTOCOL_SHIFT) & MaskLeftOverBitsUntil64(PROTOCOL_LEN));
            return_schema.Dtype = static_cast<DataTypeOfMacroColumn>((packed_scheme >> DTYPE_SHIFT) & MaskLeftOverBitsUntil64(DTYPE_LEN));
            return_schema.Version = static_cast<uint8_t>((packed_scheme >> VERSION_SHIFT) & MaskLeftOverBitsUntil64(VERSION_LEN));
            return_schema.Flags = static_cast<SchemaFlags>((packed_scheme >> FLAGS_SHIFT) & MaskLeftOverBitsUntil64(FLAGS_LEN));

            return SchemaSelfValidation(return_schema);
        }

        static constexpr SchemaProtocols GetProtocolForColumn(
            const InitialRegionalProtocol& conc_conf,
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
            default: return SchemaProtocols::PRIVATE_REGION;
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
            default: return DataTypeOfMacroColumn::UINT64_T;
            }
        }

    };
    
}