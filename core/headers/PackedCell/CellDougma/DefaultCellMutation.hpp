
#pragma once 
#include <array>
#include <utility>
#include "Mode32CellModels.hpp"

namespace PredictedAdaptedEncoding
{
    struct DefaultCellMutation
    {



    };
    

    struct Mutation64_t : public DefaultCellMutation
    {

        static constexpr bool IsCellABoundedRetryCandidate_(const PackedCell64_t::AuthoritiveCellView& auth_view) noexcept
        {
            if (
                !auth_view.IsCellValid || 
                auth_view.ContractOfValue != ContractOfConcurrency::BOUNDED_RETRY_CAS_NO_CLAIMED
            )
            {
                return false;
            }
            return true;
        }
        
        static constexpr packed64_t AddDeltaInAtomicUnsignedCell(
            uint32_t delta, 
            packed64_t packed_cell,
            uint64_t* updated_count = nullptr
        ) noexcept
        {
            const PackedCell64_t::AuthoritiveCellView auth_view = PackedCell64_t::GetAuthoritiveViewsForACell(packed_cell);
            if (
                !IsCellABoundedRetryCandidate_(auth_view) ||
                auth_view.CellValueDataType != InternalDataTypePolicy::UNSIGNED
            )
            {
                return false;
            }
            const uint64_t winded_count = auth_view.Raw48BitInCellData < PackedCell64_t::BIT_FAMILY_48_SENTINAL ?
                (auth_view.Raw48BitInCellData + delta) : (auth_view.Raw32BitInCellData + delta);

            if (updated_count)
            {
                return *updated_count = winded_count;
            }
            
            switch (auth_view.CellMode)
            {
            case PackedMode::VALUE32:
                return PackedCell64_t::Compose32BitFamilyPackedCell(
                    static_cast<uint32_t>(winded_count),
                    auth_view.InCellClock16,
                    auth_view.InCellMeta16
                );
            case PackedMode::VALUE48:
                return PackedCell64_t::Compose48BitFamilyPackedCell(
                    static_cast<uint32_t>(winded_count),
                    auth_view.InCellClock16
                );
            
            default:
                return PackedCell64_t::PACKED_CELL_SENTINAL;
            }
        }
    };
    


}