#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"

namespace PredictedAdaptedEncoding
{


    bool FabricToAPCLinker::BindExternalRawFabricBacking_(
        uint64_t* words_raw,
        uint16_t cell_count,
        VagueTemoraryPremativeFabric* fabric_owner,
        uint64_t fabric_slot_idx
    ) noexcept
    {
        if (
            !words_raw ||
            !fabric_owner ||
            !APCDataStructure::IsCapacityOfAPCValid(cell_count) ||
            !HashIdConstructror::IsValidAPCSlotIdx(fabric_slot_idx)
        )
        {
            return false;
        }

        const APCSegmentPoolRange range_of_this_apc = FabricOwnerPtr_->GetSegmentPoolBegainEndForSingleAPCDescription(fabric_slot_idx);
        if (
            !range_of_this_apc.IsValid ||
            range_of_this_apc.EndIndex - range_of_this_apc.BeginIndex != cell_count
        )
        {
            return false;
        }
        RangeOfThisAPCInSlab_ = range_of_this_apc;
        CapacityOfThisAPC_ = cell_count;
        FabricOwnerPtr_ = fabric_owner;
        IdxOfThisAPCInFabric_ = fabric_slot_idx;
        return true;
    }


    void FabricToAPCLinker::ReleseFabricBindingOnly_() noexcept
    {
        APCSegmentPoolRange default_null_range{};
        RangeOfThisAPCInSlab_ = default_null_range;
        CapacityOfThisAPC_ = UNSIGNED_ZERO;
        FabricOwnerPtr_ = nullptr;
        IdxOfThisAPCInFabric_ = SIZE_MAX;
    }

    void FabricToAPCLinker::SetFabricOwnerForGlobalAPC(VagueTemoraryPremativeFabric* fabric_owner) noexcept
    {
        FabricOwnerPtr_ = fabric_owner;
    }

    bool FabricToAPCLinker::CompareExchangeSequentiallRevertInFail(
        uint16_t starting_idx_in_apc,
        uint16_t sequential_number_of_cells,
        const uint64_t* source_cells
    ) noexcept
    {
        if (
            !RangeOfThisAPCInSlab_.IsValid ||
            !IsValidAPCRange(starting_idx_in_apc, sequential_number_of_cells)
        )
        {
            return false;
        }

        return FabricOwnerPtr_->CompareExchangeStrongSequentiallyOrRevert(
            (RangeOfThisAPCInSlab_.BeginIndex + starting_idx_in_apc), 
            sequential_number_of_cells, 
            source_cells
        );
    }

    bool FabricToAPCLinker::ForceCopyToAPCFromBuffer(
        uint16_t starting_idx_in_apc,
        uint16_t sequential_number_of_cells,
        const uint64_t* source_cells
    ) noexcept
    {
        if (
            !RangeOfThisAPCInSlab_.IsValid ||
            !IsValidAPCRange(starting_idx_in_apc, sequential_number_of_cells)
        )
        {
            return false;
        }
        
        return FabricOwnerPtr_->ForceNxLenMemCopy(
            (RangeOfThisAPCInSlab_.BeginIndex + starting_idx_in_apc), 
            sequential_number_of_cells, 
            source_cells
        );
    }

    bool FabricToAPCLinker::CopyFromAPCToBuffer(
        uint16_t starting_idx_in_apc,
        uint16_t sequential_number_of_cells,
        uint64_t* return_buffer,
        bool atomic_required
    ) noexcept
    {
        if (
            !RangeOfThisAPCInSlab_.IsValid ||
            !IsValidAPCRange(starting_idx_in_apc, sequential_number_of_cells)
        )
        {
            return false;
        }
        
        return FabricOwnerPtr_->ReadASnapShotFromSlab(
            (RangeOfThisAPCInSlab_.BeginIndex + starting_idx_in_apc), 
            sequential_number_of_cells, 
            return_buffer,
            atomic_required
        );
    }
}