#pragma once 
#include <span>
#include "../FabricOrchestrators/HandleAndRetirement.hpp"

namespace BidirectionalInMemGraph
{

    class FabricConstructor
    {
    protected:
        uint64_t* SlabBasePtr_{nullptr};

        uint32_t PerAPCRuntimeCellCount_{UNSIGNED_ZERO};
        uint64_t CountOfAPC_{UNSIGNED_ZERO};

        size_t SlabCellCount_{UNSIGNED_ZERO};
        size_t SegmentPoolBegin_{CoreOfFabricCoordinator::FABRIC_UNIT_COUNT};

        uint64_t HorizontalEdgeBeginIdx_{UNSIGNED_ZERO};
        uint64_t VerticalEdgeBeginIdx_{UNSIGNED_ZERO};
        uint64_t HandleTableBeginIndex_{UNSIGNED_ZERO};

        uint8_t MaxDirectParentsPerAxis_{UNSIGNED_ZERO};
        uint16_t EdgeTableRecordWidth_{UNSIGNED_ZERO};

        std::atomic<bool> FabricInitialized_{false};
        std::atomic<bool> InitializationInProgress_{false};
        RawPackedCellAllocator AllocatorOfFabric_{};
        using SeqLockedOperation = FabricToAPCLinker::SeqLockedOperation;

        using DSA = DescriptionOfAPC;

        bool ReadAFabricU64Directly(
            size_t slab_index,
            uint64_t& return_value
        ) noexcept;

        bool AtomicallyLoadReadAUnit(
            size_t slab_index,
            uint64_t& return_value
        ) noexcept;
        
        void DirectlyStoreFabricUnit64(size_t slab_index, uint64_t fabric_unit) noexcept;

        void AtomicallyStoreU64Fab(
            size_t slab_index, uint64_t fabric_unit, 
            std::memory_order mem_order = std::memory_order_release
        ) noexcept;

        bool CompareExchangeStrongFromFabric(
            size_t slab_index, 
            uint64_t& expected_packed_cell, 
            uint64_t desired_packed_cell,
            std::memory_order mem_order_success = std::memory_order_acq_rel,
            std::memory_order mem_order_failure = std::memory_order_acquire
        ) noexcept;

        bool CompareExchangeWeakInSlab(
            size_t slab_index, 
            uint64_t& expected_packed_cell, 
            uint64_t desired_packed_cell,
            std::memory_order mem_order_success = std::memory_order_acq_rel,
            std::memory_order mem_order_failure = std::memory_order_acquire
        ) noexcept;

        bool ForceNxLenMemCopy(
            size_t slab_starting_idx, 
            size_t number_of_cells, 
            const uint64_t* desired_cells
        ) noexcept;

        /// @param sync_idx_of_buffer It is the buffer index caller dosent need to know slab index 
        bool ReadBufferwithSyncAtomicIndex(
            size_t slab_starting_idx, 
            size_t sequential_number_of_cells,
            uint64_t* return_buffer,
            uint64_t sync_idx_of_buffer
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

    class APCHandleAndRetirement : public FabricConstructor
    {
    protected:
        uint64_t* GetAPCGenerationPtr_(uint32_t slot) noexcept;

        bool InitializeAPCGenerationTable_() noexcept;

        bool OpenAPCGeneration_(uint32_t slot, uint32_t generation) noexcept;

        bool CloseAPCGeneration_(uint32_t slot, uint32_t generation) noexcept;

        bool AdvanceClosedAPCGeneration_(uint32_t slot, uint32_t& generation_new) noexcept;

        std::optional<uint32_t> ReadFirstFreeAPCIdx_() noexcept;

        void UpdateFirstFreeIdx_(uint64_t& expected_value, uint64_t desired_value) noexcept;


    };




}