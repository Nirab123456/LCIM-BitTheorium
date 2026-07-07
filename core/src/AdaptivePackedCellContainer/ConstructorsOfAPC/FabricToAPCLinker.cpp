#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"

namespace PredictedAdaptedEncoding
{


    bool FabricToAPCLinker::BindExternalRawFabricBacking_(
        packed64_t* raw_cells_ptr,
        uint16_t cell_count,
        VagueTemoraryPremativeFabric* fabric_owner,
        uint64_t fabric_slot_idx
    ) noexcept
    {
        if (
            !raw_cells_ptr ||
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
        FabricBackend_ = true;
        return true;
    }


    void FabricToAPCLinker::ReleseFabricBindingOnly_() noexcept
    {
        APCSegmentPoolRange default_null_range{};
        RangeOfThisAPCInSlab_ = default_null_range;
        CapacityOfThisAPC_ = UNSIGNED_ZERO;
        FabricOwnerPtr_ = nullptr;
        IdxOfThisAPCInFabric_ = APCDataStructure::APC_SIZE_SENTINAL;
        FabricBackend_ = false;
    }

    void FabricToAPCLinker::SetFabricOwnerForGlobalAPC(VagueTemoraryPremativeFabric* fabric_owner) noexcept
    {
        FabricOwnerPtr_ = fabric_owner;
    }

    bool FabricToAPCLinker::ClaimAndCopyToAPCFromBuffer(
        uint16_t starting_idx_in_apc,
        uint16_t sequential_number_of_cells,
        const packed64_t* source_cells
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
            source_cells,
            false
        );
    }

    bool FabricToAPCLinker::ForceCopyToAPCFromBuffer(
        uint16_t starting_idx_in_apc,
        uint16_t sequential_number_of_cells,
        const packed64_t* source_cells
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
            source_cells,
            true
        );
    }

    bool FabricToAPCLinker::CopyFromAPCToBuffer(
        uint16_t starting_idx_in_apc,
        uint16_t sequential_number_of_cells,
        packed64_t* return_buffer
    ) noexcept
    {
        if (
            !RangeOfThisAPCInSlab_.IsValid ||
            !IsValidAPCRange(starting_idx_in_apc, sequential_number_of_cells)
        )
        {
            return false;
        }
        
        return FabricOwnerPtr_->ReadASnapShotFromSlab_(
            (RangeOfThisAPCInSlab_.BeginIndex + starting_idx_in_apc), 
            sequential_number_of_cells, 
            return_buffer
        );
    }
}