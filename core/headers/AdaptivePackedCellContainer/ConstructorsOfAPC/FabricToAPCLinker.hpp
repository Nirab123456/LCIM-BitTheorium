#pragma once
#include <functional>
#include "../APCOrchestrators/HeaderOrchestrator.hpp"

namespace PredictedAdaptedEncoding
{
    
class VagueTemoraryPremativeFabric;
class AdaptivePackedCellContainer;

class FabricToAPCLinker : public APCDataStructure
{

protected:
    VagueTemoraryPremativeFabric* FabricOwnerPtr_{nullptr};
    uint64_t IdxOfThisAPCInFabric_{FABRIC_CELL_SENTINAL};
    APCSegmentPoolRange RangeOfThisAPCInSlab_{};
    uint32_t CapacityOfThisAPC_{UNSIGNED_ZERO};
    
    void ReleseFabricBindingOnly_() noexcept;

public:

    void FreeAll() noexcept;

    void SetFabricOwnerForGlobalAPC(VagueTemoraryPremativeFabric* fabric_owner) noexcept;

    bool CompareExchangeSequentiallRevertInFail(
        uint32_t starting_idx_in_apc,
        uint8_t sequential_number_of_cells,
        const uint64_t* source_cells
    ) noexcept;

    bool ForceCopyToAPCFromBuffer(
        uint32_t tarting_idx_in_apc,
        uint8_t sequential_number_of_cells,
        const uint64_t* source_cells
    ) noexcept;

    bool CopyFromAPCToBuffer(
        uint32_t starting_idx_in_apc,
        uint8_t sequential_number_of_cells,
        uint64_t* return_buffer,
        bool atomic_required = true
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

    uint64_t AtomicallyReadLongLongAPCUnit(uint64_t idx) noexcept;

    bool IsFabricBackend() const noexcept
    {
        return FabricOwnerPtr_ != nullptr;
    }

    uint64_t GetFabricSlotIndex() const noexcept
    {
        return IdxOfThisAPCInFabric_;
    }

    VagueTemoraryPremativeFabric* GetFabricOwner() noexcept
    {
        return FabricOwnerPtr_;
    }

    static constexpr uint32_t PayloadBegin() noexcept
    {
        return METACELL_COUNT;
    }

    constexpr bool IsValidAPCRange(size_t starting_idx_in_slab, uint16_t sequential_number_of_cells) noexcept
    {
        return RangeOfThisAPCInSlab_.IsValid && 
            FabricOwnerPtr_ &&
            sequential_number_of_cells != UNSIGNED_ZERO &&
            starting_idx_in_slab <= CapacityOfThisAPC_ &&
            sequential_number_of_cells <= (CapacityOfThisAPC_ - starting_idx_in_slab);
    }

    


    std::optional<uint64_t> TryToOwnSealedFingerprintIdentity() noexcept;

    bool RestoreClaimedIdentityFingerprint(
        uint64_t sealed_fingerprint
    ) noexcept;

};
    
    
    
}