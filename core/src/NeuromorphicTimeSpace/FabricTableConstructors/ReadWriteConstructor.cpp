#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace PredictedAdaptedEncoding
{


    void ReadWriteConstructor::MakeAndStoreFabricMetaValue48_(
        FabricMetaIndicies fabric_meta_idx, 
        uint64_t value, 
        ContractOfConcurrency access_contract,
        LocalityPolicy cell_locality,
        WildCardOfPackedCell attribute
    )noexcept
    {
        const size_t slab_index = static_cast<size_t>(fabric_meta_idx);
        if (
            slab_index >= APCDataStructure::METACELL_COUNT || 
            !IsDesiredIndexValidInSLab(slab_index)
        )
        {
            return;
        }

        const packed64_t desired_packed_cell = PackedCell64_t::MakeTypedFabricValidPackedCell(
            TypeFamily::VALUE48, access_contract,
            FabricTableSegmentClasses::CONTROL_HEADER,
            cell_locality,
            InternalDataTypePolicy ::UNSIGNED,
            attribute,
            value
        );

        StorePackedCellUncheckedDirectly(slab_index, desired_packed_cell);
        
    }


    bool ReadWriteConstructor::ReadASnapShotFromSlab(
        size_t slab_starting_idx, 
        size_t sequential_number_of_cells, 
        const packed64_t* return_buffer
    ) noexcept
    {
        if (
            !IsDesiredIndexValidInSLab(slab_starting_idx) ||
            !return_buffer ||
            sequential_number_of_cells == UNSIGNED_ZERO ||
            sequential_number_of_cells > SlabCellCount_ - slab_starting_idx
        )
        {
            return false;
        }

        std::memcpy(
            &return_buffer,
            &SlabBasePtr_[slab_starting_idx],
            sequential_number_of_cells * sizeof(packed64_t)
        );

        return true;
    }


    constexpr uint64_t ReadWriteConstructor::UpdateACounterAtomically(size_t desired_idx, uint32_t delta) noexcept
    {
        if (!IsDesiredIndexValidInSLab(desired_idx))
        {
            return PackedCell64_t::PACKED_CELL_SENTINAL;
        }

        for (size_t tries = 0; tries < DEFAULT_MAX_TRIES; tries++)
        {
            packed64_t expected_cell = AtomicallyLoadReadCompletePackedCell(desired_idx);
            uint64_t updated_count = UNSIGNED_ZERO;
            const packed64_t updated_cell = Mutation64_t::AddDeltaInAtomicUnsignedCell(delta, expected_cell, &updated_count);
            if (updated_cell == PackedCell64_t::PACKED_CELL_SENTINAL)
            {
                return PackedCell64_t::PACKED_CELL_SENTINAL;
            }
            
            if (CompareExchangeStrongFromFabric(desired_idx, expected_cell, updated_cell, MoClaimSuccess, MoClaimFailure))
            {
                return updated_count;
            }
        }
        return PackedCell64_t::PACKED_CELL_SENTINAL;
    }


    /// @brief An Invalid Cell Is not Claimable - Reinitate as valid cell
    /// @param slab_index 
    /// @param expected_cell 
    /// @return 
    JustifyClaimCas ReadWriteConstructor::TryClaimACellInSlab(PackedCell64_t::AuthoritiveCellView& expected_cell_auth_view, packed64_t* desired_packed_cell) noexcept
    {
        if (!expected_cell_auth_view.IsCellValid)
        {
            return JustifyClaimCas::INVALID_CELL;
        }

        if (expected_cell_auth_view.SlabIndexOfPackeCell == APCDataStructure::APC_SIZE_SENTINAL)
        {
            return JustifyClaimCas::OUT_OF_BOUND;
        }
        
        for (size_t i = 0; i < DEFAULT_MAX_TRIES; i++)
        {
            packed64_t currennt_expected_cell = AtomicallyLoadReadCompletePackedCell(expected_cell_auth_view.SlabIndexOfPackeCell);
            if (currennt_expected_cell == PackedCell64_t::PACKED_CELL_SENTINAL)
            {
                return JustifyClaimCas::CELL_SENTINAL_STATE;
            }
            if (currennt_expected_cell != expected_cell_auth_view.RawCell)
            {
                return JustifyClaimCas::INVALID_USE_OF_METHOD;
            }
            
            const packed64_t desired_cell = PackedCell64_t::SetLocalityInPacked(expected_cell_auth_view.RawCell, LocalityPolicy::CLAIMED);
            if (CompareExchangeWeakInSlab(expected_cell_auth_view.SlabIndexOfPackeCell, currennt_expected_cell, desired_cell))
            {
                if (desired_packed_cell)
                {
                    *desired_packed_cell = desired_cell;
                }

                return JustifyClaimCas::SUCCESS;
            }
        }

        return JustifyClaimCas::CAS_LOOP_RANOUT;
        
    }


    //Integrate AtomicAdaptiveBackoff
    // add CAS_FAILURE_COUNT
    bool ReadWriteConstructor::UpdateValidPairedOccupancyApproxAtomically_(
        LocalityPolicy candidate_to_update, uint64_t desired_occupancy_value,
        bool force_update, clk16_t pair_version
    ) noexcept
    {
        const FabricMetaIndicies desired_occupancy_low_idx = CoreOfFabricCoordinator::GetDesiredLowIdxOfOccupancyPairFromLocality(candidate_to_update);

        if (desired_occupancy_low_idx == FabricMetaIndicies::EOF_FABRIC_HEADER)
        {
            return false;
        }

        const size_t low_idx = static_cast<size_t>(desired_occupancy_low_idx);
        const size_t high_idx = low_idx + 1;

        const std::pair<packed64_t, packed64_t> low32_and_probable_high32 = PairedVersionedCellModelOfMode32::GetPairOfLow32FAndHigh32SFromUnsigned64ForFabric(
            desired_occupancy_value, pair_version,
            LocalityPolicy::PUBLISHED,
            FabricTableSegmentClasses::CONTROL_HEADER
        );

        auto ForceUpdate = [&](){
            AtomicallyStorePackedCellUnchecked(low_idx, low32_and_probable_high32.first);
            AtomicallyStorePackedCellUnchecked(high_idx, low32_and_probable_high32.second);
            return true;
        };
        
        if (force_update)
        {
            return ForceUpdate();
        }

        PackedCell64_t::AuthoritiveCellView low32_half_view{};
        PackedCell64_t::AuthoritiveCellView high32_half_view{};
        std::optional<uint64_t> maybe_desired_candidate_occupancy = ReadOccupancyApproxFromPairedIfValid(candidate_to_update, &low32_half_view, &high32_half_view);

        if (!maybe_desired_candidate_occupancy|| *maybe_desired_candidate_occupancy == PackedCell64_t::PACKED_CELL_SENTINAL)
        {
            return ForceUpdate();
        }
    
        if (low32_half_view.LocalityOfCell == LocalityPolicy::CLAIMED || high32_half_view.LocalityOfCell == LocalityPolicy::CLAIMED)
        {
            return false;
        }


        //just cmpx  low
        if (
            *maybe_desired_candidate_occupancy <= BIT_FAMILY_32_SENTINAL && 
            low32_and_probable_high32.second == PackedCell64_t::PACKED_CELL_SENTINAL
        )
        {
            packed64_t expected = low32_half_view.RawCell;
            const packed64_t desired = low32_and_probable_high32.first;
            for (size_t i = 0; i < DEFAULT_MAX_TRIES; i++)
            {
                if (CompareExchangeStrongFromFabric(low_idx, expected, desired))
                {
                    AtomicallyStorePackedCellUnchecked(high_idx, low32_and_probable_high32.second);
                    return true;
                }

                if (PackedCell64_t::ExtractLocalityFromCell(expected) == LocalityPolicy::CLAIMED)
                {
                    return false;
                }
                
            }
            //intehrate failure count and AtomicAdaptiveBackoff
            return false;
        }

        //double cas 
        if ((low32_half_view.IsCellValid && high32_half_view.IsCellValid) || *maybe_desired_candidate_occupancy > BIT_FAMILY_32_SENTINAL)
        {
            packed64_t expected_low = low32_half_view.RawCell;
            const packed64_t desired_claimed_low = PackedCell64_t::SetLocalityInPacked(low32_half_view.RawCell, LocalityPolicy::CLAIMED);

            auto RestoreLow = [&]()
            {
                AtomicallyStorePackedCellUnchecked(low_idx, low32_half_view.RawCell);
                return false;
            };

            for (size_t i = 0; i < DEFAULT_MAX_TRIES; i++)
            {
                if (CompareExchangeStrongFromFabric(low_idx, expected_low, desired_claimed_low))
                {
                    packed64_t expected_high = high32_half_view.RawCell;

                    for (size_t j = 0; j < DEFAULT_MAX_TRIES; j++)
                    {
                        if (CompareExchangeStrongFromFabric(high_idx, expected_high, low32_and_probable_high32.second))
                        {
                            AtomicallyStorePackedCellUnchecked(low_idx, low32_and_probable_high32.first);
                            return true;
                        }

                        if (PackedCell64_t::ExtractLocalityFromCell(expected_high) == LocalityPolicy::CLAIMED)
                        {
                            return RestoreLow();
                        }
                    }

                    return RestoreLow();
                }

                if (PackedCell64_t::ExtractLocalityFromCell(expected_low) == LocalityPolicy::CLAIMED)
                {
                    return false;
                }
            }
        }

        return ForceUpdate(); 
    }


}