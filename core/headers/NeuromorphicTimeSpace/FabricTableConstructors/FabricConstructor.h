#pragma once 
#include <span>
#include "../FabricOrchestrators/EdgeTableConf.hpp"

namespace BidirectionalInMemGraph
{

    class FabricConstructor
    {
    protected:
        uint64_t* SlabBasePtr_{nullptr};

        size_t SlabCellCount_{UNSIGNED_ZERO};
        uint32_t PerAPCRuntimeCellCount_{UNSIGNED_ZERO};
        uint64_t CountOfAPC_{UNSIGNED_ZERO};
        uint8_t SlabId_{UNSIGNED_ZERO};

        size_t SegmentPoolBegin_{CoreOfFabricCoordinator::FABRIC_UNIT_COUNT};
        size_t SegmentPoolEnd_{CoreOfFabricCoordinator::FABRIC_UNIT_COUNT};
        
        uint64_t HashBucketCount_{UNSIGNED_ZERO};
        uint64_t ThreadTableCapacity_{UNSIGNED_ZERO};


        std::atomic<bool> FabricInitialized_{false};
        std::atomic<bool> InitializationInProgress_{false};
        RawPackedCellAllocator AllocatorOfFabric_{};


    public:

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

        bool AtomicallyCopyFromBufferToFabric(
            size_t slab_starting_idx, 
            uint32_t number_of_cells, 
            const uint64_t* desired_cells
        ) noexcept;

        bool ReadASnapShotFromSlab(
            size_t slab_starting_idx, 
            size_t sequential_number_of_cells,
            uint64_t* return_buffer,
            bool atomic_required = false
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