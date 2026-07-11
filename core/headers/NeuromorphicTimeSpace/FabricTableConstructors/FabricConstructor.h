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
        uint64_t* SlabBasePtr_{nullptr};

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

    private:

        template <size_t EXTENT>
        bool ForceNxLenMemCopy(
            size_t slab_starting_idx,
            size_t sequential_number_of_cells,
            std::span<const uint64_t, EXTENT> desired_cells,
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
            const uint64_t (&source_cells)[NUMBER_OF_CELLS],
            bool force_update = false
        ) noexcept
        {
            return ForceNxLenMemCopy(
                slab_starting_idx,
                sequential_number_of_cells,
                std::span<const uint64_t, NUMBER_OF_CELLS>(source_cells),
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
            const std::array<uint64_t, NUMBER_OF_CELLS>& source_cells
        ) noexcept
        {
            return ForceNxLenMemCopy(
                slab_starting_idx,
                sequential_number_of_cells,
                std::span<const uint64_t, NUMBER_OF_CELLS>(source_cells),
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
            const std::array<uint64_t, NUMBER_OF_CELLS>& source_cells
        ) noexcept
        {
            return ForceNxLenMemCopy(
                slab_starting_idx,
                sequential_number_of_cells,
                std::span<const uint64_t, NUMBER_OF_CELLS>(source_cells),
                true
            );
        }

    public:

        uint64_t ReadCompletePackedCellDirectly(size_t slab_index) noexcept;

        constexpr uint64_t AtomicallyLoadReadCompletePackedCell(size_t slab_index) noexcept;
        
        bool ReadFabricMetaCellViewAtomically(FabricMetaIndicies fabric_meta_idx, PackedCell64_t::AuthoritiveCellView& meta_cell_view_address) noexcept;

        constexpr void StorePackedCellUncheckedDirectly(size_t slab_index, uint64_t packed_cell) noexcept;

        constexpr void AtomicallyStorePackedCellUnchecked(size_t slab_index, uint64_t packed_cell, std::memory_order mem_order = MoStoreSeq_) noexcept;

        /// @brief Do not change default memory order unless have total idea
        /// @param expected_packed_cell ->ADDRESS
        /// @return bool
        constexpr bool CompareExchangeStrongFromFabric(
            size_t slab_index, 
            uint64_t& expected_packed_cell, 
            uint64_t desired_packed_cell,
            std::memory_order mem_order_success = MoClaimSuccess,
            std::memory_order mem_order_failure = MoClaimFailure
        ) noexcept;

        /// @brief Do not change default memory order unless have total idea
        /// @param expected_packed_cell ->ADDRESS
        /// @return bool
        constexpr bool CompareExchangeWeakInSlab(
            size_t slab_index, 
            uint64_t& expected_packed_cell, 
            uint64_t desired_packed_cell,
            std::memory_order mem_order_success = MoClaimSuccess,
            std::memory_order mem_order_failure = MoLoad_
        ) noexcept;

        std::optional<uint64_t> ReadOccupancyApproxFromPairedIfValid(
            LocalityPolicy desired_occupancy_class,
            PackedCell64_t::AuthoritiveCellView* low_half_view_ptr = nullptr,
            PackedCell64_t::AuthoritiveCellView* high_half_view_ptr = nullptr
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
            const uint64_t* desired_cells,
            bool force_update = false
        ) noexcept;

        constexpr bool IsDesiredIndexValidInSLab(size_t desired_idx) noexcept
        {
            if (SlabBasePtr_ && desired_idx < SlabCellCount_)
            {
                return true;
            }
            return false;
        }

    };



}