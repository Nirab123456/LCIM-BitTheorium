
#pragma once 
#include <array>
#include <utility>
#include "CellValidators.hpp"

namespace PredictedAdaptedEncoding
{
    struct ContractContents : public CellValidators
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
            DIRECT_READ = 1,
            MUTATION_PROHIBATED = 2,
            MEM_ORDER_ACQUIRE = 3,
            MEM_ORDER_RELESE = 4,
            ACQUIRE_CLAIM_GUARD = 5,
            RELESE_CLAIM_GUARD = 6,
            WHOLE_CELL_CAS = 7,
            CAS_LOOP_RMW = 8,
            DIRECT_STORE = 9,
            INVALID = 10
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
            COUNTER_VALUE_48_CAS = 1,
            PAYLOAD_VALUE32_CLAIMED = 2,
            PAYLOAD_VALUE48_CLAIMED = 3,
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
            WildCardOfPackedCell WildCard = WildCardOfPackedCell::UNASSIGNED_UNUSED_NANNULL;
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


        static constexpr ContractOfConcurrency HiddenConcurrencyForModel32(Model32Subclass subclass32) noexcept
        {
            switch (subclass32)
            {
            case Model32Subclass::LOW_OF_PAIRED_VERSIONED_CELL:
            case Model32Subclass::HIGH_OF_PAIRED_VERSIONED_CELL:
                return ContractOfConcurrency::BOUNDED_RETRY_CAS_NO_CLAIMED;
            
            case Model32Subclass::SELF_CLASS:
            case Model32Subclass::UNCLOCKED_1x8_PLUS_2x4:
                return ContractOfConcurrency::CLAIMED_GURDED;
            default:
                return ContractOfConcurrency::UNASSIGNED_UNUSED_NANNULL;
            }
        }

        static constexpr ContractOfConcurrency HiddenConcurrencyModel48Contract(Model48Subclass subclass48) noexcept
        {
            switch (subclass48)
            {
            case Model48Subclass::SUBDIVISION16x3_INTERNAL_CELL_MODEL:
                return ContractOfConcurrency::BOUNDED_RETRY_CAS_NO_CLAIMED;

            case Model48Subclass::FOUR_SUBDIVISION_2x16_AND_2x8:
            case Model48Subclass::PURE_TIMER_48:
            case Model48Subclass::SELF_CLASS:
                return ContractOfConcurrency::CLAIMED_GURDED;
            
            default:
                return ContractOfConcurrency::UNASSIGNED_UNUSED_NANNULL;
            }
        }

    };

    struct ConstructorOfContractKey : public ContractContents
    {
        static constexpr Meta16Files FileBookFromMeta16(meta16_t meta16) noexcept
        {
            Meta16Files contract_key{};
            if (
                meta16 == META_16_SENTINAL || 
                meta16 == UNSIGNED_ZERO
            )
            {
                return contract_key;
            }
            contract_key.Owner = static_cast<OwnershipPolicy>(ExtractOwnershipFromMeta16_(meta16));
            contract_key.CellMode = static_cast<PackedMode>(ExtractModeFromCell(meta16));
            contract_key.DataType = static_cast<InternalDataTypePolicy>(ExtractDataTypeFromMeta16_(meta16));
            contract_key.Locality = static_cast<LocalityPolicy>(ExtractLocalityFromMeta16_(meta16));

            if(
                !IsKnownOwner(contract_key.Owner) ||
                !IsKnownDataType(contract_key.DataType) ||
                !IsKnownLocality(contract_key.Locality)
            )
            {
                return contract_key;
            }

            const tag8_t region = ExtractRegionFromMeta16_(meta16);
            if (contract_key.Owner == OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER)
            {
                contract_key.APCRegion = static_cast<APCPagedNodeSegmentClasses>(region);
                contract_key.FabricRegion = FabricTableSegmentClasses::NONE;
                if (!IsKnowAPCRegion(contract_key.APCRegion))
                {
                    return contract_key;
                }
            }
            else if (contract_key.Owner == OwnershipPolicy::NEUROMORPHIC_SPACE_TIME_FABRIC)
            {
                contract_key.FabricRegion = static_cast<FabricTableSegmentClasses>(region);
                contract_key.APCRegion = APCPagedNodeSegmentClasses::NONE;
                if (!IsKnownFabricRegion(contract_key.FabricRegion))
                {
                    return contract_key;
                }
            }
            
            //MODE
            const tag8_t subclass = ExtractSubClassOrContractFromMeta16_(meta16);
            switch (contract_key.CellMode)
            {

            case PackedMode::MODEL32:
                contract_key.SubClassOfModel32 = static_cast<Model32Subclass>(subclass);
                if (!IsKnownModel32Subclass(contract_key.SubClassOfModel32))
                {
                    return contract_key;
                }
                contract_key.Concurrency = HiddenConcurrencyForModel32(contract_key.SubClassOfModel32);
                break;

            case PackedMode::MODEL48:
                contract_key.SubClassOfModel48 = static_cast<Model48Subclass>(subclass);
                if (!IsKnownModel48Subclass(contract_key.SubClassOfModel48))
                {
                    return contract_key;
                }
                contract_key.Concurrency = HiddenConcurrencyModel48Contract(contract_key.SubClassOfModel48);
                break;

            case PackedMode::VALUE32:
            case PackedMode::VALUE48:
                contract_key.Concurrency = static_cast<ContractOfConcurrency>(subclass);
                if (!IsKnownConcurrencyContractForValue(contract_key.Concurrency))
                {
                    return contract_key;
                }
                break;
            
            default:
                return contract_key;
            }

            if (!IsKnownConcurrencyContractForValue(contract_key.Concurrency))
            {
                return contract_key;
            }
            
            contract_key.StructurallyValid = true;
            return contract_key;
            
        }

        static constexpr Meta16Files FileBookFromPackedCell(packed64_t packed_cell) noexcept
        {
            if (IsRawCell(packed_cell))
            {
                return Meta16Files{};
            }
            return FileBookFromMeta16(ExtractMeta16fromPackedCell(packed_cell));
        }


        static constexpr Meta16Files APCValueKeyConstruction(
            APCPagedNodeSegmentClasses region,
            TypeFamily mode,
            ContractOfConcurrency concurrency,
            LocalityPolicy locality,
            InternalDataTypePolicy dtype,
            WildCardOfPackedCell attribute 
        ) noexcept
        {
            return Meta16Files{
                OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER,
                static_cast<PackedMode>(mode),
                FabricTableSegmentClasses::NONE,
                region,
                Model32Subclass::UNASSIGNED_UNUSED_NANNULL,
                Model48Subclass::UNASSIGNED_UNUSED_NANNULL,
                concurrency,
                dtype,
                attribute,
                locality,
                true
            };
        }

        static constexpr Meta16Files APCModel32Key(
            APCPagedNodeSegmentClasses region,
            PackedMode mode,
            Model32Subclass model32,
            LocalityPolicy locality,
            InternalDataTypePolicy dtype,
            WildCardOfPackedCell attribute 
        ) noexcept
        {
            return Meta16Files{
                OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER,
                mode,
                FabricTableSegmentClasses::NONE,
                region,
                model32,
                Model48Subclass::UNASSIGNED_UNUSED_NANNULL,
                HiddenConcurrencyForModel32(model32),
                dtype,
                attribute,
                locality,
                true
            };
        }

        static constexpr Meta16Files APCModel48KeyConstruction(
            APCPagedNodeSegmentClasses region,
            Model48Subclass model48,
            LocalityPolicy locality,
            InternalDataTypePolicy dtype,
            WildCardOfPackedCell attribute 
        ) noexcept
        {
            return Meta16Files{
                OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER,
                PackedMode::MODEL48,
                FabricTableSegmentClasses::NONE,
                region,
                Model32Subclass::UNASSIGNED_UNUSED_NANNULL,
                model48,
                HiddenConcurrencyModel48Contract(model48),
                dtype,
                attribute,
                locality,
                true
            };
        }

        static constexpr Meta16Files FabricAnyValueKeyConstruction(
            FabricTableSegmentClasses region,
            TypeFamily mode,
            ContractOfConcurrency concurrency,
            LocalityPolicy locality,
            InternalDataTypePolicy dtype,
            WildCardOfPackedCell attribute 
        ) noexcept
        {
            return Meta16Files{
                OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER,
                static_cast<PackedMode>(mode),
                region,
                APCPagedNodeSegmentClasses::NONE,
                Model32Subclass::UNASSIGNED_UNUSED_NANNULL,
                Model48Subclass::UNASSIGNED_UNUSED_NANNULL,
                concurrency,
                dtype,
                attribute,
                locality,
                true
            };
        }


        static constexpr Meta16Files FabricModel32Key(
            FabricTableSegmentClasses region,
            PackedMode mode,
            Model32Subclass model32,
            LocalityPolicy locality,
            InternalDataTypePolicy dtype,
            WildCardOfPackedCell attribute 
        ) noexcept
        {
            return Meta16Files{
                OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER,
                mode,
                region,
                APCPagedNodeSegmentClasses::NONE,
                model32,
                Model48Subclass::UNASSIGNED_UNUSED_NANNULL,
                HiddenConcurrencyForModel32(model32),
                dtype,
                attribute,
                locality,
                true
            };
        }

        static constexpr Meta16Files FabricModel48Key(
            FabricTableSegmentClasses region,
            PackedMode mode,
            Model48Subclass model48,
            LocalityPolicy locality,
            InternalDataTypePolicy dtype,
            WildCardOfPackedCell attribute 
        ) noexcept
        {
            return Meta16Files{
                OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER,
                mode,
                region,
                APCPagedNodeSegmentClasses::NONE,
                Model32Subclass::UNASSIGNED_UNUSED_NANNULL,
                model48,
                HiddenConcurrencyModel48Contract(model48),
                dtype,
                attribute,
                locality,
                true
            };
        }
    };
    
}