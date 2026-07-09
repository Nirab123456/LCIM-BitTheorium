
#pragma once 
#include <array>
#include <utility>
#include "ContractInvarients.hpp"

namespace PredictedAdaptedEncoding
{


    struct ContractValidator : public ConstructorOfContractKey
    {
        static constexpr bool SameRegionDomainNode(const Meta16Files& expected, const Meta16Files& got) noexcept
        {
            if (expected.Owner == OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER)
            {
                return expected.APCRegion == APCPagedNodeSegmentClasses::NULLNAN || 
                    expected.APCRegion == got.APCRegion;
            }

            if (expected.Owner == OwnershipPolicy::NEUROMORPHIC_SPACE_TIME_FABRIC)
            {
                return expected.FabricRegion == FabricTableSegmentClasses::NULLNAN ||
                    expected.FabricRegion == got.FabricRegion;
            }
            return false;
        }

        static constexpr bool SameMeta16RecordFile(const Meta16Files& expected, const Meta16Files& got) noexcept
        {
            if (
                !expected.StructurallyValid ||
                !got.StructurallyValid
            )
            {
                return false;
            }
            if (
                expected.Owner != got.Owner ||
                expected.CellMode != got.CellMode ||
                !SameRegionDomainNode(expected, got) ||
                expected.Attribute != got.Attribute ||
                expected.Locality != got.Locality ||
                expected.Concurrency != got.Concurrency
            )
            {
                return false;
            }
            
            switch (got.CellMode)
            {
            case PackedMode::VALUE32:
            case PackedMode::VALUE48:
                return true;
            case PackedMode::MODEL32:
                return expected.SubClassOfModel32 == got.SubClassOfModel32;
            case PackedMode::MODEL48:
                return expected.SubClassOfModel48 == got.SubClassOfModel48;
            default:
                return false;
            }
        }

        static constexpr bool IsValidContract(const SwornCellContract& contract) noexcept
        {
            return contract.IsValid &&
                contract.IdOfContract != listOfContract::INVALID &&
                contract.Mutation != MutationOparations::INVALID &&
                contract.KeyOfContract.StructurallyValid &&
                IsKnownOwner(contract.KeyOfContract.Owner) &&
                contract.KeyOfContract.CellMode != PackedMode::UNASSIGNED_UNUSED_NANNULL;
        }
    };


    struct ContractConstructor : public ConstructorOfContractKey
    {
        static constexpr uint8_t TOTAL_AMOUNT_OF_CONTRACTS = 22;

        static constexpr SwornCellContract MakeAContract(
            listOfContract desired_contract,
            Meta16Files contract_key,
            MutationOparations mutation,
            RelationKernel relation = RelationKernel::NONE
        ) noexcept
        {
            return SwornCellContract{desired_contract, contract_key, mutation, relation, true};
        }

        static constexpr std::array<SwornCellContract, TOTAL_AMOUNT_OF_CONTRACTS>DefaultSavedContractsOfPackedCell() noexcept
        {
            return 
            {
                /// RELESE APC META -> IDELE + PUBLISHED
                MakeAContract(
                    listOfContract::APC_META_VALUE_RELESE, 
                    APCValueKeyConstruction(
                        APCPagedNodeSegmentClasses::META_HEADER, TypeFamily::VALUE48,
                        ContractOfConcurrency::LAST_WRITIER_WIN_NO_CAS_RMW, LocalityPolicy::IDLE,
                        InternalDataTypePolicy::UNSIGNED, AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL
                    ),
                    MutationOparations::MEM_ORDER_RELESE
                ),//1
                MakeAContract(
                    listOfContract::APC_META_VALUE_RELESE, 
                    APCValueKeyConstruction(
                        APCPagedNodeSegmentClasses::META_HEADER, TypeFamily::VALUE48,
                        ContractOfConcurrency::LAST_WRITIER_WIN_NO_CAS_RMW, LocalityPolicy::PUBLISHED,
                        InternalDataTypePolicy::UNSIGNED, AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL
                    ),
                    MutationOparations::MEM_ORDER_RELESE
                ),//2
                ///COUNTER
                MakeAContract(
                    listOfContract::COUNTER_VALUE_48_CAS,
                    APCValueKeyConstruction(
                        APCPagedNodeSegmentClasses::META_HEADER, TypeFamily::VALUE48,
                        ContractOfConcurrency::BOUNDED_RETRY_CAS_NO_CLAIMED, LocalityPolicy::PUBLISHED,
                        InternalDataTypePolicy::UNSIGNED, AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL
                    ),
                    MutationOparations::CAS_LOOP_RMW
                ),//3

                /// APC PAYLOAD VALUE 
                MakeAContract(
                    listOfContract::PAYLOAD_VALUE32_CLAIMED,
                    APCValueKeyConstruction(
                        APCPagedNodeSegmentClasses::WILD_CARD_ALL_REGION_CONTRACT, TypeFamily::VALUE32,
                        ContractOfConcurrency::CLAIMED_GURDED,  LocalityPolicy::IDLE,
                        InternalDataTypePolicy::UNSIGNED, AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL
                    ),
                    MutationOparations::RELESE_CLAIM_GUARD
                ),//4

                MakeAContract(
                    listOfContract::PAYLOAD_VALUE32_CLAIMED,
                    APCValueKeyConstruction(
                        APCPagedNodeSegmentClasses::WILD_CARD_ALL_REGION_CONTRACT, TypeFamily::VALUE32,
                        ContractOfConcurrency::CLAIMED_GURDED,  LocalityPolicy::PUBLISHED,
                        InternalDataTypePolicy::UNSIGNED, AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL
                    ),
                    MutationOparations::RELESE_CLAIM_GUARD
                ),//5

                MakeAContract(
                    listOfContract::PAYLOAD_VALUE48_CLAIMED,
                    APCValueKeyConstruction(
                        APCPagedNodeSegmentClasses::WILD_CARD_ALL_REGION_CONTRACT, TypeFamily::VALUE48,
                        ContractOfConcurrency::CLAIMED_GURDED,  LocalityPolicy::IDLE,
                        InternalDataTypePolicy::UNSIGNED, AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL
                    ),
                    MutationOparations::RELESE_CLAIM_GUARD
                ),//6

                MakeAContract(
                    listOfContract::PAYLOAD_VALUE48_CLAIMED,
                    APCValueKeyConstruction(
                        APCPagedNodeSegmentClasses::WILD_CARD_ALL_REGION_CONTRACT, TypeFamily::VALUE48,
                        ContractOfConcurrency::CLAIMED_GURDED,  LocalityPolicy::PUBLISHED,
                        InternalDataTypePolicy::UNSIGNED, AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL
                    ),
                    MutationOparations::RELESE_CLAIM_GUARD
                ),//7

                ///LAYOUT-OF-APC
                MakeAContract(
                    listOfContract::APC_LAYOUT_MODEL48,
                    APCModel48KeyConstruction(
                        APCPagedNodeSegmentClasses::META_HEADER, Model48Subclass::FOUR_SUBDIVISION_2x16_AND_2x8,
                        LocalityPolicy::PUBLISHED, InternalDataTypePolicy::UNSIGNED,
                        AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL
                    ),
                    MutationOparations::RELESE_CLAIM_GUARD,
                    RelationKernel::LAYOUT_BUFFER_LOCAL
                ),//8

                //OCCUPANCY MODEL APC
                MakeAContract(
                    listOfContract::APC_OCCUPANCY_MODEL48,
                    APCModel48KeyConstruction(
                        APCPagedNodeSegmentClasses::META_HEADER, Model48Subclass::FOUR_SUBDIVISION_2x16_AND_2x8,
                        LocalityPolicy::PUBLISHED, InternalDataTypePolicy::UNSIGNED,
                        AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL
                    ),
                    MutationOparations::CAS_LOOP_RMW,
                    RelationKernel::OCCUPANCY_BUFFER_LOCAL
                ),//9

                /// FABRID HEADER
                MakeAContract(
                    listOfContract::FABRIC_META_VALUE_48_RELEASE,
                    FabricAnyValueKeyConstruction(
                        FabricTableSegmentClasses::CONTROL_HEADER, TypeFamily::VALUE48,
                        ContractOfConcurrency::CLAIMED_GURDED, LocalityPolicy::PUBLISHED,
                        InternalDataTypePolicy::UNSIGNED, AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL
                    ),
                    MutationOparations::MEM_ORDER_RELESE
                ),//10
                MakeAContract(
                    listOfContract::FABRIC_RECORDBOOK_VALUE48_RAW,
                    FabricAnyValueKeyConstruction(
                        FabricTableSegmentClasses::SLAB_RECORD_MAP, TypeFamily::VALUE48,
                        ContractOfConcurrency::RAW_PRIVATE, LocalityPolicy::PUBLISHED,
                        InternalDataTypePolicy::UNSIGNED, AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL
                    ),
                    MutationOparations::MUTATION_PROHIBATED
                ),//11
                MakeAContract(
                    listOfContract::FABRIC_DESCRIPTOR_VALUE48,
                    FabricAnyValueKeyConstruction(
                        FabricTableSegmentClasses::APC_HANDLE_DESCRIPTOR, TypeFamily::VALUE48,
                        ContractOfConcurrency::CLAIMED_GURDED, LocalityPolicy::PUBLISHED,
                        InternalDataTypePolicy::UNSIGNED, AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL
                    ),
                    MutationOparations::RELESE_CLAIM_GUARD
                )




            };
        }



    };
    
    
}