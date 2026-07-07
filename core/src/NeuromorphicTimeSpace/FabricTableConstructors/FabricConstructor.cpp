#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace PredictedAdaptedEncoding
{

    packed64_t FabricConstructor::ReadCompletePackedCellDirectly(size_t slab_index) noexcept
    {
        if (!IsDesiredIndexValidInSLab(slab_index))
        {
            return PackedCell64_t::PACKED_CELL_SENTINAL;
        }
        const packed64_t desired_cell_raw = SlabBasePtr_[slab_index];

        return desired_cell_raw;
    } 

    constexpr packed64_t FabricConstructor::AtomicallyLoadReadCompletePackedCell(size_t slab_index) noexcept
    {
        if (!IsDesiredIndexValidInSLab(slab_index))
        {
            return PackedCell64_t::PACKED_CELL_SENTINAL;
        }
        std::atomic_ref<const packed64_t> packed_cell_ref(SlabBasePtr_[slab_index]);
        const packed64_t desired_cell_raw = packed_cell_ref.load(MoLoad_);

        return desired_cell_raw;
    }

    constexpr void FabricConstructor::StorePackedCellUncheckedDirectly(size_t slab_index, packed64_t packed_cell) noexcept
    {
        if (!IsDesiredIndexValidInSLab(slab_index))
        {
            return;
        }
        SlabBasePtr_[slab_index] = packed_cell;
    }

    constexpr void FabricConstructor::AtomicallyStorePackedCellUnchecked(
        size_t slab_index, packed64_t packed_cell,
        std::memory_order mem_order
    ) noexcept
    {
        if (!IsDesiredIndexValidInSLab(slab_index))
        {
            return;
        }
        std::atomic_ref<packed64_t> packed_cell_ref(SlabBasePtr_[slab_index]);
        packed_cell_ref.store(packed_cell, mem_order);
        packed_cell_ref.notify_all();
    }

    constexpr bool FabricConstructor::CompareExchangeStrongFromFabric(
        size_t slab_index, 
        packed64_t& expected_packed_cell, 
        packed64_t desired_packed_cell,
        std::memory_order mem_order_success,
        std::memory_order mem_order_failure
    ) noexcept
    {
        if (!IsDesiredIndexValidInSLab(slab_index))
        {
            return false;
        }
        std::atomic_ref<packed64_t> packed_cell_ref(SlabBasePtr_[slab_index]);
        return packed_cell_ref.compare_exchange_strong(expected_packed_cell, desired_packed_cell, mem_order_success, mem_order_failure);
    }

    constexpr bool FabricConstructor::CompareExchangeWeakInSlab(
        size_t slab_index, 
        packed64_t& expected_packed_cell, 
        packed64_t desired_packed_cell,
        std::memory_order mem_order_success,
        std::memory_order mem_order_failure
    ) noexcept
    {
        if (!IsDesiredIndexValidInSLab(slab_index))
        {
            return false;
        }
        std::atomic_ref<packed64_t> packed_cell_ref(SlabBasePtr_[slab_index]);
        return packed_cell_ref.compare_exchange_weak(expected_packed_cell, desired_packed_cell, mem_order_success, mem_order_failure);
    }

    bool FabricConstructor::ReadFabricMetaCellViewAtomically(FabricMetaIndicies fabric_meta_idx, PackedCell64_t::AuthoritiveCellView& meta_cell_view_address) noexcept
    {
        const size_t meta_index_in_slab = static_cast<size_t>(fabric_meta_idx);
        if (
            meta_index_in_slab >= APCDataStructure::METACELL_COUNT ||
            !IsDesiredIndexValidInSLab(meta_index_in_slab)
        )
        {
            return false;
        }

        const packed64_t packed_meta_cell = AtomicallyLoadReadCompletePackedCell(meta_index_in_slab);
        meta_cell_view_address = PackedCell64_t::GetAuthoritiveViewsForACell(packed_meta_cell);

        return true;        
    }

    std::optional<uint64_t> FabricConstructor::ReadOccupancyApproxFromPairedIfValid(
        LocalityPolicy desired_occupancy_class,
        PackedCell64_t::AuthoritiveCellView* low_half_view_ptr,
        PackedCell64_t::AuthoritiveCellView* high_half_view_ptr
    ) noexcept
    {
        const FabricMetaIndicies desired_occupancy_low_idx = CoreOfFabricCoordinator::GetDesiredLowIdxOfOccupancyPairFromLocality(desired_occupancy_class);
        if (desired_occupancy_low_idx == FabricMetaIndicies::EOF_FABRIC_HEADER)
        {
            return std::nullopt;
        }

        const size_t desired_low_idx = static_cast<size_t>(desired_occupancy_low_idx);
        const size_t desired_high_idx = static_cast<size_t>(desired_occupancy_low_idx) + 1;

        packed64_t raw_occ_low = AtomicallyLoadReadCompletePackedCell(desired_low_idx);
        packed64_t raw_occ_high = AtomicallyLoadReadCompletePackedCell(desired_high_idx);

        
        auto result = PairedVersionedCellModelOfMode32::GetFullUnsigned64FromPairedVersionedCell(raw_occ_low, raw_occ_high, low_half_view_ptr, high_half_view_ptr);

        if (low_half_view_ptr)
        {
            low_half_view_ptr->SlabIndexOfPackeCell =  desired_low_idx;
        } 

        if (high_half_view_ptr)
        {
            high_half_view_ptr->SlabIndexOfPackeCell = desired_high_idx;
        }

        return result;
    }


    bool FabricConstructor::ClaimNxSequentialPackedCellStrong(
        size_t claim_starting_idx_in_slab, 
        size_t claim_order_cell_count
    ) noexcept
    {
        if (
            !IsDesiredIndexValidInSLab(claim_starting_idx_in_slab)||
            claim_order_cell_count == UNSIGNED_ZERO || 
            claim_order_cell_count > MAXIMUM_CLAIMABLE_COUNT_SEQUENTIALLY ||
            claim_order_cell_count > SlabCellCount_ - claim_starting_idx_in_slab
        )
        {
            return false;
        }
        
        uint8_t changed_amount = UNSIGNED_ZERO;
        HeaderOrchestrator::DefaultMemCopyBuffer packed_cell_buffer{};
        HeaderOrchestrator::BuildNullMemCopyBuffer(packed_cell_buffer);

        for (uint8_t idx_inc = 0; idx_inc < claim_order_cell_count; idx_inc++)
        {
            const size_t current_slab_idx = static_cast<size_t>(idx_inc + claim_starting_idx_in_slab);
            packed64_t expected_cell = AtomicallyLoadReadCompletePackedCell(current_slab_idx);

            packed_cell_buffer[idx_inc] = expected_cell;

            if (!PackedCell64_t::IsCellClaimableFromThisCaller(expected_cell))
            {
                break;
            }
            
            const packed64_t desired_cell = PackedCell64_t::SetLocalityInPacked(expected_cell, LocalityPolicy::CLAIMED);

            if (!CompareExchangeStrongFromFabric(
                current_slab_idx,
                expected_cell,
                desired_cell
            ))
            {
                break;
            }
            
            changed_amount = idx_inc + 1;
        }

        if (changed_amount != claim_order_cell_count)
        {
            for (uint8_t recover_idx = 0; recover_idx < changed_amount; recover_idx++)
            {
                StorePackedCellUncheckedDirectly(claim_starting_idx_in_slab + recover_idx, packed_cell_buffer[recover_idx]);
            }

            return false;
        }
        
        return true;
    }


    bool FabricConstructor::ForceNxLenMemCopy(
        size_t slab_starting_idx, 
        size_t number_of_cells, 
        const packed64_t* desired_cells,
        bool force_update
    ) noexcept
    {
        if (
            !IsDesiredIndexValidInSLab(slab_starting_idx) ||
            !desired_cells ||
            number_of_cells == UNSIGNED_ZERO ||
            number_of_cells > SlabCellCount_ - slab_starting_idx
        )
        {
            return false;
        }

        if (!force_update)
        {
            const bool claim_ok = ClaimNxSequentialPackedCellStrong(slab_starting_idx, number_of_cells);
            if (!claim_ok)
            {
                return false;
            }
        }

        std::memcpy(
            &SlabBasePtr_[slab_starting_idx],
            desired_cells,
            number_of_cells * sizeof(packed64_t)
        );

        return true;
    }



}