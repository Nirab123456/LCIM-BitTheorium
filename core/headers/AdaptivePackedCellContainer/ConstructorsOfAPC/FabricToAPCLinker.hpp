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

    packed64_t* OwnedRawBackingCells_{nullptr};
    APCBackingCellAtomicRefViewTemp* OwnedBackingView_{nullptr};
    VagueTemoraryPremativeFabric* FabricOwnerPtr_{nullptr};
    uint64_t IdxOfThisAPCInFabric_{APCDataStructure::APC_SIZE_SENTINAL};
    bool FabricBackend_{false};
    bool FabricObjectOwnedByFabric_{false};

/// UPDATE Candidates
    size_t CapacityOfThisAPC_{UNSIGNED_ZERO};
    Timer48 LocalTimer48_;
    std::function<void(const char*, const char*)> APCLogger_;
    std::vector<std::vector<uint64_t>> SOABitmapForAPC_;
///

    void ReleseFabricBindingOnly_() noexcept;

    bool GetThisAPCRangeInSlab_(
        APCSegmentPoolRange& return_range,
        size_t starting_idx_in_apc = UNSIGNED_ZERO,
        size_t width = UNSIGNED_ZERO
    ) noexcept;

public:
    APCBackingCellAtomicRefViewTemp* BackingPtr{nullptr};

    void FreeAll() noexcept;

    void SetFabricOwnerForGlobalAPC(VagueTemoraryPremativeFabric* fabric_owner) noexcept;

    bool ClaimAndCopyToAPCFromBuffer(
        size_t starting_idx_in_apc,
        size_t sequential_number_of_cells,
        const packed64_t* source_cells
    ) noexcept;

    bool ForceCopyToAPCFromBuffer(
        size_t starting_idx_in_apc,
        size_t sequential_number_of_cells,
        const packed64_t* source_cells
    ) noexcept;

    bool CopyFromAPCToBuffer(
        size_t starting_idx_in_apc,
        size_t sequential_number_of_cells,
        packed64_t* return_buffer
    ) noexcept;

    bool BindExternalRawFabricBacking_(
        packed64_t* raw_cells_ptr,
        size_t cell_count,
        VagueTemoraryPremativeFabric* fabric_owner,
        uint64_t fabric_slot_idx,
        bool object_owned_by_fabric
    ) noexcept;

    bool IsFabricBackend() const noexcept
    {
        return FabricBackend_;
    }

    uint64_t GetFabricSlotIndex() const noexcept
    {
        return IdxOfThisAPCInFabric_;
    }

    VagueTemoraryPremativeFabric* GetFabricOwner() noexcept
    {
        return FabricOwnerPtr_;
    }

    bool IsBound() const noexcept
    {
        return BackingPtr != nullptr && CapacityOfThisAPC_ >= METACELL_COUNT;
    }

    size_t PayloadCapacity() const noexcept
    {
        return CapacityOfThisAPC_ > METACELL_COUNT ? (CapacityOfThisAPC_ - METACELL_COUNT) : 0u;
    }

    bool ValidMetaIdx(MetaIndexOfAPCNode idx) noexcept
    {
        return BackingPtr && static_cast<size_t>(idx) < CapacityOfThisAPC_ && static_cast<size_t>(idx) < METACELL_COUNT;
    }

    static constexpr uint32_t PayloadBegin() noexcept
    {
        return METACELL_COUNT;
    }


};
    
    
    
}