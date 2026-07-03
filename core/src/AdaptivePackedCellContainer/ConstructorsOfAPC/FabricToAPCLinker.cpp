#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"

namespace PredictedAdaptedEncoding
{


    bool FabricToAPCLinker::BindExternalRawFabricBacking_(
        packed64_t* raw_cells_ptr,
        size_t cell_count,
        VagueTemoraryPremativeFabric* fabric_owner,
        uint64_t fabric_slot_idx,
        bool object_owned_by_fabric
    ) noexcept
    {
        if (!raw_cells_ptr || cell_count < MINIMUM_APC_CAPACITY || !fabric_owner)
        {
            return false;
        }
        
        APCBackingCellAtomicRefViewTemp* backing_mem_view = APCBackingCellAtomicRefViewTemp::BuildBackingViewOverCells_(raw_cells_ptr, cell_count);
        if (!backing_mem_view)
        {
            return false;
        }

        BackingPtr = backing_mem_view;
        OwnedBackingView_ = backing_mem_view;
        OwnedRawBackingCells_ = nullptr;
        CapacityOfThisAPC_ = cell_count;
        FabricOwnerPtr_ = fabric_owner;
        IdxOfThisAPCInFabric_ = fabric_slot_idx;
        FabricBackend_ = true;
        FabricObjectOwnedByFabric_ = object_owned_by_fabric;
        return true;
    }


    void FabricToAPCLinker::ReleseFabricBindingOnly_() noexcept
    {
        if (OwnedBackingView_)
        {
            APCBackingCellAtomicRefViewTemp::FreeBackingView_(OwnedBackingView_, CapacityOfThisAPC_);
        }

        OwnedBackingView_ = nullptr;
        BackingPtr = nullptr;
        OwnedRawBackingCells_ = nullptr;
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

    bool FabricToAPCLinker::IsThisAPCValidRange_(
        size_t starting_idx_in_apc,
        size_t width,  
        APCSegmentPoolRange* return_range
    ) noexcept
    {
        if (!FabricOwnerPtr_)
        {
            return false;
        }
        
        const APCSegmentPoolRange range_of_this_apc = FabricOwnerPtr_->GetSegmentPoolBegainEndForSingleAPCDescription(IdxOfThisAPCInFabric_);
        if (!range_of_this_apc.IsVAlid)
        {
            return false;
        }

        if (starting_idx_in_apc > CapacityOfThisAPC_ || width > (CapacityOfThisAPC_ - starting_idx_in_apc))
        {
            return false;
        }
        

        if (return_range)
        {
            *return_range = range_of_this_apc;
        }

        return range_of_this_apc.IsVAlid && range_of_this_apc.BeginIndex + starting_idx_in_apc + width <= range_of_this_apc.EndIndex;
    }


    bool FabricToAPCLinker::ClaimAndCopyToAPCFromBuffer(
        size_t starting_idx_in_apc,
        size_t sequential_number_of_cells,
        const packed64_t* source_cells
    ) noexcept
    {
        APCSegmentPoolRange range_of_this_apc{};
        if (!IsThisAPCValidRange_(starting_idx_in_apc, sequential_number_of_cells, &range_of_this_apc) || source_cells == nullptr)
        {
            return false;
        }

        return FabricOwnerPtr_->ForceNxLenMemCopy(
            (range_of_this_apc.BeginIndex + starting_idx_in_apc), 
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
        APCSegmentPoolRange range_of_this_apc{};
        if (!IsThisAPCValidRange_(starting_idx_in_apc, sequential_number_of_cells, &range_of_this_apc) || source_cells == nullptr)
        {
            return false;
        }
        
        return FabricOwnerPtr_->ForceNxLenMemCopy(
            (range_of_this_apc.BeginIndex + starting_idx_in_apc), 
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
        APCSegmentPoolRange range_of_this_apc{};
        if (!IsThisAPCValidRange_(starting_idx_in_apc, sequential_number_of_cells, &range_of_this_apc) || return_buffer == nullptr)
        {
            return false;
        }
        
        return FabricOwnerPtr_->ForceNxLenMemCopy(
            (range_of_this_apc.BeginIndex + starting_idx_in_apc), 
            sequential_number_of_cells, 
            return_buffer,
            true
        );
    }
}