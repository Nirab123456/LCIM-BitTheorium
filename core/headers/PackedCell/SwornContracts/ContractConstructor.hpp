
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
                        APCPagedNodeSegmentClasses::META_HEADER, PackedMode::VALUE48,
                        ContractOfConcurrency::LAST_WRITIER_WIN_NO_CAS_RMW, LocalityPolicy::IDLE,
                        InternalDataTypePolicy::UNSIGNED, AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL
                    ),
                    MutationOparations::RELEASE_STORE_ONLY
                ),
                MakeAContract(
                    listOfContract::APC_META_VALUE_RELESE, 
                    APCValueKeyConstruction(
                        APCPagedNodeSegmentClasses::META_HEADER, PackedMode::VALUE48,
                        ContractOfConcurrency::LAST_WRITIER_WIN_NO_CAS_RMW, LocalityPolicy::PUBLISHED,
                        InternalDataTypePolicy::UNSIGNED, AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL
                    ),
                    MutationOparations::RELEASE_STORE_ONLY
                ),

                MakeAContract(
                    listOfContract::APC_COUNTER_VALUE_48_CAS,
                    APCValueKeyConstruction(
                        APCPagedNodeSegmentClasses::META_HEADER, PackedMode::VALUE48,
                        ContractOfConcurrency::BOUNDED_RETRY_CAS_NO_CLAIMED, LocalityPolicy::PUBLISHED,
                        InternalDataTypePolicy::UNSIGNED, AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL
                    ),
                    MutationOparations::CAS_LOOP_RMW
                )

            };
        }



    };
    
    
}