#pragma once
#include <functional>
#include "../APCOrchestrators/HeaderOrchestrator.hpp"

namespace BidirectionalInMemGraph
{
    
class VagueTemoraryPremativeFabric;
class AdaptivePackedCellContainer;

class FabricToAPCLinker 
{

protected:
    VagueTemoraryPremativeFabric* FabricOwnerPtr_{nullptr};
    RangeOfAPC RangeOfThisAPCInSlab_{};
    uint32_t CapacityOfThisAPC_{UNSIGNED_ZERO};
    uint64_t* RawAPCBasePtr_{nullptr};
    
    uint32_t APCSlotIdx_{APCDataStructure::APC_INDEX_BOUND_SENTINAL};
public:
    void ReleseFabricBindingOnly_() noexcept;

    bool ForceCopyToAPCFromBuffer(
        uint32_t tarting_idx_in_apc,
        uint32_t sequential_number_of_cells,
        const uint64_t* source_cells
    ) noexcept;

    bool CopyFromAPCToBuffer(
        uint32_t starting_idx_in_apc,
        uint32_t sequential_number_of_cells,
        uint64_t* return_buffer
    ) noexcept;

    bool CompareExchangeStrongFromAPC(
        size_t apc_idx, 
        uint64_t& expected_unit, 
        uint64_t desired_unit,
        std::memory_order mem_order_success = std::memory_order_acq_rel,
        std::memory_order mem_order_failure = std::memory_order_acquire
    ) noexcept;

    bool BindExternalRawFabricBacking_(
        uint64_t* raw_cells_ptr,
        uint32_t cell_count,
        VagueTemoraryPremativeFabric* fabric_owner,
        uint64_t fabric_slot_idx
    ) noexcept;

    bool AtomicallyReadLongLongAPCUnit(
        uint64_t idx,
        uint64_t& return_value
    ) noexcept;

    void AtomicallyWriteU64ToAPC(
        uint64_t idx,
        const uint64_t& value
    ) noexcept;

    VagueTemoraryPremativeFabric* GetFabricOwner() noexcept
    {
        return FabricOwnerPtr_;
    }

    static constexpr uint32_t PayloadBegin() noexcept
    {
        return APCDataStructure::METACELL_COUNT;
    }

    bool IsThisAPCValid() noexcept
    {
        return
            FabricOwnerPtr_ &&
            RangeOfThisAPCInSlab_.IsValid;
    }

    uint32_t GetThisSlotIdx() noexcept
    {
        return APCSlotIdx_;
    }

};
    
    
    
}