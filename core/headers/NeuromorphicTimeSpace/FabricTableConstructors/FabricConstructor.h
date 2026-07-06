#pragma once 
#include <span>
#include "../FabricOrchestrators/RecordBookConf.hpp"
#include "../FabricOrchestrators/DescriptionOfAPC.hpp"
#include "../FabricOrchestrators/HashTableConf.hpp"

namespace PredictedAdaptedEncoding
{

    class FabricConstructor
    {
    protected:
        packed64_t* SlabBasePtr_{nullptr};

        size_t SlabCellCount_{UNSIGNED_ZERO};
        uint16_t PerAPCRuntimeCellCount_{UNSIGNED_ZERO};
        uint64_t CountOfAPC_{UNSIGNED_ZERO};
        uint8_t SlabId_{UNSIGNED_ZERO};

        size_t SegmentPoolBegin_{APCDataStructure::METACELL_COUNT};
        size_t SegmentPoolEnd_{APCDataStructure::METACELL_COUNT};
        
        uint64_t HashBucketCount_{UNSIGNED_ZERO};
        uint64_t RelationRecordCount_{UNSIGNED_ZERO};
        uint64_t DeviceViewRecordCount_{UNSIGNED_ZERO};
        uint64_t ThreadTableCapacity_{UNSIGNED_ZERO};


        std::atomic<bool> FabricInitialized_{false};
        std::atomic<bool> InitializationInProgress_{false};
        RawPackedCellAllocator AllocatorOfFabric_{};

        static constexpr size_t DefaultFabricAlignment16Cell_(size_t value) noexcept
        {
            const uint8_t alignment_value_15 = 16 - 1;
            return (value + alignment_value_15) & ~static_cast<size_t>(alignment_value_15);
        }

        /// @brief ONLY: Use for Initialiazation ONLY
        void MakeAndStoreFabricMetaValue48_(
            FabricMetaIndicies fabric_meta_idx, uint64_t value, 
            AccessContractOfValue access_contract = AccessContractOfValue::CAS_RMW,
            LocalityPolicy cell_locality = LocalityPolicy::PUBLISHED,
            AttributePolicy attribute = AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL
        )noexcept;

    private:

        template <size_t EXTENT>
        bool ForceNxLenMemCopy(
            size_t slab_starting_idx,
            size_t sequential_number_of_cells,
            std::span<const packed64_t, EXTENT> desired_cells,
            bool force_update = false
        ) noexcept
        {
            return ForceNxLenMemCopy(
                slab_starting_idx,
                sequential_number_of_cells,
                desired_cells.data(),
                force_update
            );
        };

        template <size_t NUMBER_OF_CELLS>
        bool ForceNxMemCopyPtrPlusSize_(
            size_t slab_starting_idx,
            size_t sequential_number_of_cells,
            const packed64_t (&source_cells)[NUMBER_OF_CELLS],
            bool force_update = false
        ) noexcept
        {
            return ForceNxLenMemCopy(
                slab_starting_idx,
                sequential_number_of_cells,
                std::span<const packed64_t, NUMBER_OF_CELLS>(source_cells),
                force_update
            );
        }
protected:

        /// @brief Copys From the Pointing Memory -> SlabBasePtr_ :: desired Number Of Cells 
        /// @param slab_starting_idx The Starting Index of SlabBasePtr_ From Where Copy Starts
        /// @param sequential_number_of_cells Number Of Packed Cells to be Copied
        /// @param source_cells ARRAY Of Desired Packed Cells
        /// @return true / false
        template <size_t NUMBER_OF_CELLS>
        bool ClaimThenMemCopyFromArray_(
            size_t slab_starting_idx,
            size_t sequential_number_of_cells,
            const std::array<packed64_t, NUMBER_OF_CELLS>& source_cells
        ) noexcept
        {
            return ForceNxLenMemCopy(
                slab_starting_idx,
                sequential_number_of_cells,
                std::span<const packed64_t, NUMBER_OF_CELLS>(source_cells),
                false
            );
        }

        /// @brief Copys From the Pointing Memory -> SlabBasePtr_ :: desired Number Of Cells 
        /// @param slab_starting_idx The Starting Index of SlabBasePtr_ From Where Copy Starts
        /// @param sequential_number_of_cells Number Of Packed Cells to be Copied
        /// @param source_cells ARRAY Of Desired Packed Cells
        /// @return true / false
        template <size_t NUMBER_OF_CELLS>
        bool ForceMemCopyFromArray_(
            size_t slab_starting_idx,
            size_t sequential_number_of_cells,
            const std::array<packed64_t, NUMBER_OF_CELLS>& source_cells
        ) noexcept
        {
            return ForceNxLenMemCopy(
                slab_starting_idx,
                sequential_number_of_cells,
                std::span<const packed64_t, NUMBER_OF_CELLS>(source_cells),
                true
            );
        }

        template<size_t NUMBER_OF_CELLS>
        bool ReadASnapShotFromSlab_(
            size_t slab_starting_idx,
            size_t sequential_count,
            std::array<packed64_t, NUMBER_OF_CELLS>& return_buffer
        ) noexcept;

    public:

        packed64_t ReadCompletePackedCellDirectly(size_t slab_index) noexcept;

        constexpr packed64_t AtomicallyLoadReadCompletePackedCell(size_t slab_index) noexcept;
        
        bool ReadFabricMetaCellViewAtomically(FabricMetaIndicies fabric_meta_idx, PackedCell64_t::AuthoritiveCellView& meta_cell_view_address) noexcept;

        constexpr void StorePackedCellUncheckedDirectly(size_t slab_index, packed64_t packed_cell) noexcept;

        constexpr void AtomicallyStorePackedCellUnchecked(size_t slab_index, packed64_t packed_cell, std::memory_order mem_order = MoStoreSeq_) noexcept;

        /// @brief Do not change default memory order unless have total idea
        /// @param expected_packed_cell ->ADDRESS
        /// @return bool
        constexpr bool CompareExchangeStrongFromFabric(
            size_t slab_index, 
            packed64_t& expected_packed_cell, 
            packed64_t desired_packed_cell,
            std::memory_order mem_order_success = MoClaimSuccess,
            std::memory_order mem_order_failure = MoClaimFailure
        ) noexcept;

        /// @brief Do not change default memory order unless have total idea
        /// @param expected_packed_cell ->ADDRESS
        /// @return bool
        constexpr bool CompareExchangeWeakInSlab(
            size_t slab_index, 
            packed64_t& expected_packed_cell, 
            packed64_t desired_packed_cell,
            std::memory_order mem_order_success = MoClaimSuccess,
            std::memory_order mem_order_failure = MoLoad_
        ) noexcept;

        JustifyClaimCas TryClaimACellInSlab(PackedCell64_t::AuthoritiveCellView& expected_cell_auth_view, packed64_t* desired_packed_cell = nullptr) noexcept;

        std::optional<uint64_t> ReadOccupancyApproxFromPairedIfValid(
            LocalityPolicy desired_occupancy_class,
            PackedCell64_t::AuthoritiveCellView* low_half_view_ptr = nullptr,
            PackedCell64_t::AuthoritiveCellView* high_half_view_ptr = nullptr
        ) noexcept;

        /// @brief UPDATES OR: Initializes PAIRED: Occupancy | Why PAIRED ? To Potentially Justify by Version OR: Internal CLOCK16 How Much Accumulatiom Diffarence Between Total and the DISTANCE: By Version or CLOCK16 
        /// @param candidate_to_update DESIRED: LocalityPolicy -> Count Want TO Be Updated | GETS TRANSLETED: To -> FabricMetaIndicies BY: CoreOfFabricCoordinator::GetDesiredLowIdxOfOccupancyPairFromLocality
        /// @param desired_occupancy_value IF: desired_occupancy_value <= UINT32_MAX ONLY -> USED: FabricMetaIndicies::FABRIC_OCCUPANCY_APPROXIMATION_LOCALITY_LOW32 || BOTH: LOW32 + HIGH32
        /// @param force_update DO NOT CHANGE TO: true untill Understand USE: IF: false -> CAS: Update || true -> ATOMIC STORE: 
        /// @return 
        bool UpdateValidPairedOccupancyApproxAtomically(
            LocalityPolicy candidate_to_update, uint64_t desired_occupancy_value,
            bool force_update = false,
            clk16_t pair_version = UNSIGNED_ZERO
        ) noexcept;


        /// @brief Try to claim N <= MAXIMUM_CLAIMABLE_COUNT_SEQUENTIALLY Packed Cells 
        /// @param slab_idx STARTING: Index -> From Where Claiming Starts
        /// @param number_of_cells Number Of CElls Wants Claimed
        bool ClaimNxSequentialPackedCellStrong(
            size_t slab_idx, 
            size_t number_of_cells
        ) noexcept;

        /// @brief Copys From the Pointing Memory -> SlabBasePtr_ :: desired Number Of Cells 
        /// @param slab_starting_idx The Starting Index of SlabBasePtr_ From Where Copy Starts
        /// @param number_of_cells Number Of Packed Cells to be Copied
        /// @param desired_cells MEMORY Of Desired Packed Cells
        /// @param force_update TRUE: Dosent Claim to LocalityPolicy::CLAIMED(Very Unsafe) FALSE: Claims To LocalityPolicy::CLAIMED 
        /// @return true / false
        bool ForceNxLenMemCopy(
            size_t slab_starting_idx, 
            size_t number_of_cells, 
            const packed64_t* desired_cells,
            bool force_update = false
        ) noexcept;

    };



}