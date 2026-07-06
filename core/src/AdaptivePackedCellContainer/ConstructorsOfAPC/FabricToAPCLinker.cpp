#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"

namespace PredictedAdaptedEncoding
{


    bool FabricToAPCLinker::BindExternalRawFabricBacking_(
        packed64_t* raw_cells_ptr,
        uint16_t cell_count,
        VagueTemoraryPremativeFabric* fabric_owner,
        uint64_t fabric_slot_idx,
        bool object_owned_by_fabric
    ) noexcept
    {
        if (
            !raw_cells_ptr ||
            cell_count < MINIMUM_APC_CAPACITY ||
            !fabric_owner ||
            APCDataStructure::IsCapacityOfAPCValid(cell_count)
        )
        {
            return false;
        }
        const APCSegmentPoolRange range_of_this_apc = FabricOwnerPtr_->GetSegmentPoolBegainEndForSingleAPCDescription(fabric_slot_idx);
        if (!range_of_this_apc.IsValid)
        {
            return false;
        }
        RangeOfThisAPCInSlab_ = range_of_this_apc;
        CapacityOfThisAPC_ = cell_count;
        FabricOwnerPtr_ = fabric_owner;
        IdxOfThisAPCInFabric_ = fabric_slot_idx;
        FabricBackend_ = true;
        FabricObjectOwnedByFabric_ = object_owned_by_fabric;
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
        FabricObjectOwnedByFabric_ = false;
    }

    void FabricToAPCLinker::SetFabricOwnerForGlobalAPC(VagueTemoraryPremativeFabric* fabric_owner) noexcept
    {
        FabricOwnerPtr_ = fabric_owner;
    }

    bool FabricToAPCLinker::ClaimAndCopyToAPCFromBuffer(
        size_t starting_idx_in_apc,
        size_t sequential_number_of_cells,
        const packed64_t* source_cells
    ) noexcept
    {
        if (
            !RangeOfThisAPCInSlab_.IsValid ||
            starting_idx_in_apc + sequential_number_of_cells >= CapacityOfThisAPC_
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
        size_t starting_idx_in_apc,
        size_t sequential_number_of_cells,
        const packed64_t* source_cells
    ) noexcept
    {
        if (
            !RangeOfThisAPCInSlab_.IsValid ||
            starting_idx_in_apc + sequential_number_of_cells >= CapacityOfThisAPC_
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
        size_t starting_idx_in_apc,
        size_t sequential_number_of_cells,
        packed64_t* return_buffer
    ) noexcept
    {
        if (
            !RangeOfThisAPCInSlab_.IsValid ||
            starting_idx_in_apc + sequential_number_of_cells >= CapacityOfThisAPC_
        )
        {
            return false;
        }
        
        return FabricOwnerPtr_->ForceNxLenMemCopy(
            (RangeOfThisAPCInSlab_.BeginIndex + starting_idx_in_apc), 
            sequential_number_of_cells, 
            return_buffer,
            true
        );
    }
}