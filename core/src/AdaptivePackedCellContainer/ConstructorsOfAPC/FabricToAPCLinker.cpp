#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"

namespace BidirectionalInMemGraph
{
    bool FabricToAPCLinker::AtomicallyReadLongLongAPCUnit(
        uint64_t idx,
        uint64_t& return_value
    ) noexcept
    {
        return 
            IsThisAPCValid() &&
            FabricOwnerPtr_->AtomicallyLoadReadAUnit(
                RangeOfThisAPCInSlab_.BeginIndex + idx,
                return_value
            );
    }

    void FabricToAPCLinker::AtomicallyWriteU64ToAPC(
        uint64_t idx,
        const uint64_t& value
    ) noexcept
    {
        if (
            !IsThisAPCValid()
        )
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
        const RangeOfAPC range_of_this_apc = fabric_owner->GetSegmentPoolRange(fabric_slot_idx);
        if (
            !range_of_this_apc.IsValid ||
            range_of_this_apc.EndIndex - range_of_this_apc.BeginIndex != cell_count
        )
        {
            return false;
        }
        APCSlotIdx_ = static_cast<uint32_t>(fabric_slot_idx);
        RawAPCBasePtr_ = words_raw;
        CapacityOfThisAPC_ = cell_count;
        FabricOwnerPtr_ = fabric_owner;
        RangeOfThisAPCInSlab_ = range_of_this_apc;
        return true;
    }


    void FabricToAPCLinker::ReleseFabricBindingOnly_() noexcept
    {
        RangeOfThisAPCInSlab_ = RangeOfAPC{};
        CapacityOfThisAPC_ = UNSIGNED_ZERO;
        FabricOwnerPtr_ = nullptr;
        RawAPCBasePtr_ = nullptr;
    }

    bool FabricToAPCLinker::ForceCopyToAPCFromBuffer(
        uint32_t starting_idx_in_apc,
        uint32_t sequential_number_of_cells,
        const uint64_t* source_cells
    ) noexcept
    {
        return 
            IsThisAPCValid() &&
            starting_idx_in_apc + sequential_number_of_cells < CapacityOfThisAPC_ &&
            FabricOwnerPtr_->ForceNxLenMemCopy(
                (RangeOfThisAPCInSlab_.BeginIndex + starting_idx_in_apc), 
                sequential_number_of_cells, 
                source_cells
            );
    }

    bool FabricToAPCLinker::CopyFromAPCToBuffer(
        uint32_t starting_idx_in_apc,
        uint32_t sequential_number_of_cells,
        uint64_t* return_buffer
    ) noexcept
    {   
        return 
            IsThisAPCValid() &&
            FabricOwnerPtr_->ReadASnapShotFromSlab(
                (RangeOfThisAPCInSlab_.BeginIndex + starting_idx_in_apc), 
                sequential_number_of_cells, 
                return_buffer
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
        return 
            apc_idx < CapacityOfThisAPC_ &&
            FabricOwnerPtr_->CompareExchangeStrongFromFabric(
                RangeOfThisAPCInSlab_.BeginIndex + apc_idx,
                expected_unit,
                desired_unit,
                mem_order_success,
                mem_order_failure
            );
    }

}