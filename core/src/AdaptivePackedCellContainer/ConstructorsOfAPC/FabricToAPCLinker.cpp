#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"

namespace BidirectionalInMemGraph
{
    bool FabricToAPCLinker::AtomicallyReadLongLongAPCUnit(
        uint64_t idx,
        uint64_t& return_value
    ) noexcept
    {
        if (!IsValidAPCRange(idx, 1))
        {
            return false;
        }

        return FabricOwnerPtr_->AtomicallyLoadReadAUnit(
            RangeOfThisAPCInSlab_.BeginIndex + idx,
            return_value
        );
    }

    void FabricToAPCLinker::AtomicallyWriteU64ToAPC(
        uint64_t idx,
        const uint64_t& value
    ) noexcept
    {
        if (!IsValidAPCRange(idx, 1))
        {
            return;
        }
        FabricOwnerPtr_->AtomicallyStoreU64Fab(
            idx + RangeOfThisAPCInSlab_.BeginIndex,
            value
        );
    }

    bool FabricToAPCLinker::BindExternalRawFabricBacking_(
        uint64_t* words_raw,
        uint32_t cell_count,
        VagueTemoraryPremativeFabric* fabric_owner,
        uint64_t fabric_slot_idx
    ) noexcept
    {
        if (
            !words_raw ||
            !fabric_owner ||
            !APCDataStructure::IsCapacityOfAPCValid(cell_count) ||
            !APCDataStructure::IsValid32BitAPCUnit(fabric_slot_idx)
        )
        {
            return false;
        }
        CapacityOfThisAPC_ = cell_count;
        FabricOwnerPtr_ = fabric_owner;
        const APCSegmentPoolRange range_of_this_apc = FabricOwnerPtr_->GetSegmentPoolBegainEndForSingleAPCDescription(fabric_slot_idx);
        if (
            !range_of_this_apc.IsValid ||
            range_of_this_apc.EndIndex - range_of_this_apc.BeginIndex != cell_count
        )
        {
            return false;
        }
        RangeOfThisAPCInSlab_ = range_of_this_apc;
        return true;
    }


    void FabricToAPCLinker::ReleseFabricBindingOnly_() noexcept
    {
        APCSegmentPoolRange default_null_range{};
        RangeOfThisAPCInSlab_ = default_null_range;
        CapacityOfThisAPC_ = UNSIGNED_ZERO;
        FabricOwnerPtr_ = nullptr;
    }

    void FabricToAPCLinker::SetFabricOwnerForGlobalAPC(VagueTemoraryPremativeFabric* fabric_owner) noexcept
    {
        FabricOwnerPtr_ = fabric_owner;
    }

    bool FabricToAPCLinker::AtomicallyCopyFromBufferToAPC(
        uint32_t starting_idx_in_apc,
        uint8_t sequential_number_of_cells,
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

        return FabricOwnerPtr_->AtomicallyCopyFromBufferToFabric(
            (RangeOfThisAPCInSlab_.BeginIndex + starting_idx_in_apc), 
            sequential_number_of_cells, 
            source_cells
        );
    }

    bool FabricToAPCLinker::ForceCopyToAPCFromBuffer(
        uint32_t starting_idx_in_apc,
        uint32_t sequential_number_of_cells,
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
        uint32_t starting_idx_in_apc,
        uint32_t sequential_number_of_cells,
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

    bool FabricToAPCLinker::CompareExchangeStrongFromAPC(
        size_t apc_idx, 
        uint64_t& expected_unit, 
        uint64_t desired_unit,
        std::memory_order mem_order_success,
        std::memory_order mem_order_failure
    ) noexcept
    {
        if (
            !RangeOfThisAPCInSlab_.IsValid ||
            !IsValidAPCRange(apc_idx, 1u)
        )
        {
            return false;
        }

        return FabricOwnerPtr_->CompareExchangeStrongFromFabric(
            RangeOfThisAPCInSlab_.BeginIndex + apc_idx,
            expected_unit,
            desired_unit,
            mem_order_success,
            mem_order_failure
        );
    }

}