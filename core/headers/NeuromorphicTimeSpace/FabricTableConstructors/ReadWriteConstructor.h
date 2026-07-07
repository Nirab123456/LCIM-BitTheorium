#pragma once 
#include "FabricConstructor.h"

namespace PredictedAdaptedEncoding
{

    class ReadWriteConstructor : public FabricConstructor
    {
    protected:

        static constexpr size_t DefaultFabricAlignment16Cell_(size_t value) noexcept
        {
            const uint8_t alignment_value_15 = 16 - 1;
            return (value + alignment_value_15) & ~static_cast<size_t>(alignment_value_15);
        }

        /// @brief UPDATES OR: Initializes PAIRED: Occupancy | Why PAIRED ? To Potentially Justify by Version OR: Internal CLOCK16 How Much Accumulatiom Diffarence Between Total and the DISTANCE: By Version or CLOCK16 
        /// @param candidate_to_update DESIRED: LocalityPolicy -> Count Want TO Be Updated | GETS TRANSLETED: To -> FabricMetaIndicies BY: CoreOfFabricCoordinator::GetDesiredLowIdxOfOccupancyPairFromLocality
        /// @param desired_occupancy_value IF: desired_occupancy_value <= UINT32_MAX ONLY -> USED: FabricMetaIndicies::FABRIC_OCCUPANCY_APPROXIMATION_LOCALITY_LOW32 || BOTH: LOW32 + HIGH32
        /// @param force_update DO NOT CHANGE TO: true untill Understand USE: IF: false -> CAS: Update || true -> ATOMIC STORE: 
        /// @return 
        bool UpdateValidPairedOccupancyApproxAtomically_(
            LocalityPolicy candidate_to_update, uint64_t desired_occupancy_value,
            bool force_update = false,
            clk16_t pair_version = UNSIGNED_ZERO
        ) noexcept;

        /// @brief ONLY: Use for Initialiazation ONLY
        void MakeAndStoreFabricMetaValue48_(
            FabricMetaIndicies fabric_meta_idx, uint64_t value, 
            ContractOfConcurrency access_contract = ContractOfConcurrency::LAST_WRITIER_WIN_NO_CAS_RMW,
            LocalityPolicy cell_locality = LocalityPolicy::PUBLISHED,
            AttributePolicy attribute = AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL
        )noexcept;

        JustifyClaimCas TryClaimACellInSlab(PackedCell64_t::AuthoritiveCellView& expected_cell_auth_view, packed64_t* desired_packed_cell = nullptr) noexcept;

    public:

        constexpr uint64_t UpdateACounterAtomically(size_t desired_idx, uint32_t delta) noexcept;

        bool ReadASnapShotFromSlab(
            size_t slab_starting_idx, 
            size_t sequential_number_of_cells, 
            const packed64_t* return_buffer
        ) noexcept;

    };



}