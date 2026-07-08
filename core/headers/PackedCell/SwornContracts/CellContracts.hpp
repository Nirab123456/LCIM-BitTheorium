
#pragma once 
#include <array>
#include <utility>
#include "CellValidators.hpp"

namespace PredictedAdaptedEncoding
{
    struct ContractContents : public PackedCellSetters
    {
        static constexpr uint16_t BitTag(tag8_t bit_value) noexcept
        {
            return bit_value < LOW16_BIT_LEN ? static_cast<uint16_t>(uint16_t{1u} << bit_value) : uint16_t{UNSIGNED_ZERO};
        }

        template <typename EnumT>
        static constexpr uint16_t BitFromEnum(EnumT value) noexcept
        {
            return BitTag(static_cast<tag8_t>(value));
        }

        enum class MutationOparations : tag8_t
        {
            VALIDATE_ONLY = 0,
            READ_ONLY_CONSUME = 1,
            RAW_STORE_ONLY = 2,
            RELEASE_STORE_ONLY = 3,
            CLAIM_THEN_PUBLISH = 4,
            WHOLE_CELL_CAS = 5,
            CAS_LOOP_RMW = 6,
            INVALID = 7
        };

        enum class FailureReasonOfContract : tag8_t
        {
            NONE = 0,
            INVALID_STRUCTURAL_CELL = 1,
            NO_CONTRACT_FOUND = 2,
            INVALID_CONTRACT_DESCRIPTOR = 3,
            LOCALITY_VIOLATION = 4,
            DATA_TYPE_VIOLATION = 5,
            ATTRIBUTE_VIOLATION = 6,
            OPERATION_VIOLATION = 7,
            CLASS_OR_OWNER_VIOLATION = 8,
            RELATION_VIOLATION = 9,
            INVALID = 10
        };

        enum class RelationKernel : tag8_t
        {
            NONE = 0,
            PAIRED_MODEL32 = 1,
            HASH_TRIPLET_LIGHT = 2,
            LAYOUT_BUFFER_LOCAL = 3,
            OCCUPANCY_BUFFER_LOCAL = 4
        };
        
        enum class listOfContract : tag8_t
        {
            APC_META_VALUE_RELESE = 0,
            APC_COUNTER_VALUE_48_CAS = 1,
            APC_PAYLOAD_VALUE32_CLAIMED = 2,
            APC_PAYLOAD_VALUE48_CLAIMED = 3,
            APC_PAYLOA_FLOAT32_CLAIMED = 4,
            APC_RAW64_INSTRUCTION_VALUE48 = 5,
            APC_LAYOUT_MODEL48 = 6,
            APC_OCCUPANCY_MODEL48 = 7,
            PAIRED_LOW_MODEL32 = 8,
            PAIRED_HIGH_MODEL32 = 9,
            FABRIC_META_VALUE_48_RELEASE = 10,
            FABRIC_RECORDBOOK_VALUE48_RAW = 11,
            FABRIC_HASH_VALUE48_CLAIMED = 12,
            FABRIC_HASH_GROUP_KEY_MODEL32 = 13,
            FABRIC_HASH_LOCK_MODEL48 = 14,
            FABRIC_DESCRIPTOR_VALUE48 = 15,
            INVALID = 16
        };

        struct Meta16Files
        {
            OwnershipPolicy Owner = OwnershipPolicy::UNASSIGNED_UNUSED_NANNULL;
            PackedMode CellMode = PackedMode::UNASSIGNED_UNUSED_NANNULL;

            FabricTableSegmentClasses FabricRegion = FabricTableSegmentClasses::NULLNAN;
            APCPagedNodeSegmentClasses APCRegion = APCPagedNodeSegmentClasses::NULLNAN;

            Model32Subclass SubClassOfModel32 = Model32Subclass::UNASSIGNED_UNUSED_NANNULL; 
            Model48Subclass SubClassOfModel48 = Model48Subclass::UNASSIGNED_UNUSED_NANNULL;
            ContractOfConcurrency Concurrency = ContractOfConcurrency::UNASSIGNED_UNUSED_NANNULL;

            InternalDataTypePolicy DataType = InternalDataTypePolicy::UNASSIGNED_UNUSED_NANNULL;
            AttributePolicy Attribute = AttributePolicy::UNASSIGNED_UNUSED_NANNULL;
            LocalityPolicy Locality = LocalityPolicy::UNASSIGNED_UNUSED_NANNULL;
            bool StructurallyValid = false;
        };

        struct SwornCellContract
        {
            listOfContract IdOfContract = listOfContract::INVALID;
            Meta16Files KeyOfContract = Meta16Files{};
            MutationOparations Mutation = MutationOparations::INVALID;
            RelationKernel Relation = RelationKernel::NONE;
            bool IsValid = false;
        };
        static_assert(sizeof(SwornCellContract) <= 2 * sizeof(uint64_t));

        struct MutationContractForCaller
        {
            listOfContract Contract = listOfContract::INVALID;
            MutationOparations Mutation = MutationOparations::INVALID;
            RelationKernel Relation = RelationKernel::NONE;
            FailureReasonOfContract FailureR = FailureReasonOfContract::INVALID;
            bool Passed = false;
        };
        static_assert(sizeof(MutationContractForCaller) < sizeof(uint64_t));

    };


    
    struct CellContracts : public ContractContents
    {
        static constexpr Meta16Files FileBookFromMeta16(meta16_t meta16) noexcept
        {
            Meta16Files this_contract_key{};
            if (
                meta16 == META_16_SENTINAL || 
                meta16 == UNSIGNED_ZERO
            )
            {
                return this_contract_key;
            }

            this_contract_key.Owner = static_cast<OwnershipPolicy>(ExtractOwnershipFromMeta16_(meta16));
            this_contract_key.CellMode = static_cast<PackedMode>(ExtractCellModeFromMETA16_U_(meta16));

            //OWNERSHIP
            switch (this_contract_key.Owner)
            {
            case OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER:
                this_contract_key.APCRegion = static_cast<APCPagedNodeSegmentClasses>(ExtractRegionFromMeta16_(meta16));
                if (
                    this_contract_key.APCRegion == APCPagedNodeSegmentClasses::NONE || 
                    this_contract_key.APCRegion == APCPagedNodeSegmentClasses::NULLNAN
                )
                {
                    return this_contract_key;
                }
                break;
            case OwnershipPolicy::NEUROMORPHIC_SPACE_TIME_FABRIC:
                this_contract_key.FabricRegion = static_cast<FabricTableSegmentClasses>(ExtractRegionFromMeta16_(meta16));
                if (
                    this_contract_key.FabricRegion == FabricTableSegmentClasses::NONE || 
                    this_contract_key.FabricRegion == FabricTableSegmentClasses::NULLNAN
                )
                {
                    return this_contract_key;
                }
                break;
            default:
                return this_contract_key;
            }

            //MODE
            switch (this_contract_key.CellMode)
            {

            case PackedMode::MODEL32:
                this_contract_key.SubClassOfModel32 = static_cast<Model32Subclass>(ExtractCellModeFromMETA16_U_(meta16));
                if (this_contract_key.SubClassOfModel32 == Model32Subclass::UNASSIGNED_UNUSED_NANNULL)
                {
                    return this_contract_key;
                }
                this_contract_key.Concurrency = ContractOfConcurrency::CLAIMED_GURDED;
                break;

            case PackedMode::MODEL48:
                this_contract_key.SubClassOfModel48 = static_cast<Model48Subclass>(ExtractCellModeFromMETA16_U_(meta16));
                if (this_contract_key.SubClassOfModel48 == Model48Subclass::UNASSIGNED_UNUSED_NANNULL)
                {
                    return this_contract_key;
                }
                this_contract_key.Concurrency = ContractOfConcurrency::CLAIMED_GURDED;
                break;

            case PackedMode::VALUE32:
            case PackedMode::VALUE48:
                this_contract_key.Concurrency = static_cast<ContractOfConcurrency>(ExtractCellModeFromMETA16_U_(meta16));
                if (this_contract_key.Concurrency == ContractOfConcurrency::UNASSIGNED_UNUSED_NANNULL)
                {
                    return this_contract_key;
                }
                break;
            
            default:
                return this_contract_key;
            }

            this_contract_key.DataType = static_cast<InternalDataTypePolicy>(ExtractValueDataTypeFromMETA16_U_(meta16));
            this_contract_key.Attribute = static_cast<AttributePolicy>(ExtractAttributeFromMeta16_(meta16));
            this_contract_key.Locality = static_cast<LocalityPolicy>(ExtractLocalityFromMETA16_U_(meta16));

            if (
                this_contract_key.DataType == InternalDataTypePolicy::UNASSIGNED_UNUSED_NANNULL ||
                this_contract_key.Attribute == AttributePolicy::UNASSIGNED_UNUSED_NANNULL ||
                this_contract_key.Locality == LocalityPolicy::UNASSIGNED_UNUSED_NANNULL
            )
            {
                return this_contract_key;
            }
            
            this_contract_key.StructurallyValid = true;
            return this_contract_key;
            
        }

        static constexpr Meta16Files FileBookFromPackedCell(packed64_t packed_cell) noexcept
        {
            return FileBookFromMeta16(ExtractMeta16fromPackedCell(packed_cell));
        }

        static constexpr SwornCellContract MakeAContract(
            listOfContract desired_contract,
            Meta16Files contract_key,
            MutationOparations mutation,
            RelationKernel relation= RelationKernel::NONE
        ) noexcept
        {
            return SwornCellContract{
                desired_contract,
                contract_key,
                mutation,
                relation
            };
        }


    };
    
}