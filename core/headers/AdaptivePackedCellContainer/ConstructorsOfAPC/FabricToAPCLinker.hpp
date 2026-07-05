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
    uint64_t IdxOfThisAPCInFabric_{PackedCell64_t::PACKED_CELL_SENTINAL};
    bool FabricBackend_{false};
    bool FabricObjectOwnedByFabric_{false};
    APCSegmentPoolRange RangeOfThisAPCInSlab_{};

/// UPDATE Candidates
    uint16_t CapacityOfThisAPC_{UNSIGNED_ZERO};
    Timer48 LocalTimer48_;
///

    void ReleseFabricBindingOnly_() noexcept;

public:

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
        uint16_t cell_count,
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

    uint16_t PayloadCapacity() const noexcept
    {
        return CapacityOfThisAPC_ > METACELL_COUNT ? static_cast<uint16_t>(CapacityOfThisAPC_ - METACELL_COUNT) : 0u;
    }

    static constexpr uint32_t PayloadBegin() noexcept
    {
        return METACELL_COUNT;
    }


};
    
    
    
}