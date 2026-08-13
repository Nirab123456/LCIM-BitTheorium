#pragma once 
#include "FabricTableConstructors/CompleteFabric.h"

namespace BidirectionalInMemGraph
{
    
    
    class SlabToFabricConverterAndCordinator : public EdgeTableConstructor
    {
    private:

        uint64_t* AllocatePackedCellRaw_(size_t count_of_cells) noexcept;
        
        void FreeRawPackedCells_(uint64_t*packed_cell_memory_ptr, size_t packed_cell_count) noexcept;

        void ResetScalarsofTheFabric_() noexcept;

        /// @brief BUILD: & INITIALIZED: All The APC Handle Descriptor With Segment Pool <-  CONSISTING: Packed CEll -> PacvkedMode::VALUE32
        void InitializeAPCDescriptorTable_() noexcept;

        /// @brief INITIALIZES: All FabricMetaIndicies
        /// @param table_directory_begin 
        /// @param table_directory_end 
        void InitializeCompleateFabricMetaIndices_(size_t record_book_begin, size_t record_book_end) noexcept;

    public:
        SlabToFabricConverterAndCordinator(/* args */) noexcept = default;

        ~SlabToFabricConverterAndCordinator() noexcept
        {
            ShutDownFabric();
        }

        SlabToFabricConverterAndCordinator(const SlabToFabricConverterAndCordinator&) = delete;
        SlabToFabricConverterAndCordinator& operator = (const SlabToFabricConverterAndCordinator&) = delete;

        void ShutDownFabric() noexcept;

        bool InitializeFabric(
            uint32_t slot_count,
            uint32_t slot_cell_count = MINIMUM_APC_CELL_COUNT,
            uint32_t fabric_thread_capacity = CoreOfFabricCoordinator::DEFAULT_THREAD_TABLE_CAPACITY
        ) noexcept;

        bool IsFabricActive() noexcept
        {
            return
                FabricInitialized_.load(std::memory_order_acquire) &&
                SlabBasePtr_ &&
                APCDataStructure::IsValid32BitAPCUnit(PerAPCRuntimeCellCount_) &&
                APCDataStructure::IsValid32BitAPCUnit(CountOfAPC_);
        }
        
    };

    class ConstructAPCIdentity : public SlabToFabricConverterAndCordinator
    {
    private:

        /// @return PREVIOUS GRAPH MUTATION VALUE RAW: MEANS: Value before change
        std::optional<uint64_t> AcquireGraphMutationFlag_(
            uint32_t apc_slot_idx,
            IAB::MemGraphFlag desired_state,
            uint32_t max_tries = DEFAULT_MAX_TRIES
        ) noexcept;

        bool WriteAcquiredAxis_(
            uint32_t apc_slot,
            const IAB::BufferOfAPCIdentity& identity,
            IAB::BidirectionalAxis axis
        ) noexcept;

        bool ReleseGraphMutationFlag_(
            uint32_t apc_slot,
            IAB::BidirectionalAxis axis,
            uint32_t max_tries = DEFAULT_MAX_TRIES
        ) noexcept;

    public:
        std::optional<StateOfAPC> ReadIdentityBufferOfAPC(
            uint32_t apc_slot,
            IAB::BufferOfAPCIdentity& identity,
            uint32_t max_tries = DEFAULT_MAX_TRIES
        ) noexcept;

        bool ReadGraphMutationFlags(
            uint32_t slot_idx,
            IAB::GraphMutationValues& values
        ) noexcept;

        bool AttachValidIdentity(uint32_t apc_idx) noexcept;

        bool InitiateRootAxis(
            uint32_t apc_slot,
            IAB::BidirectionalAxis axis
        ) noexcept;

        bool LinkTwoAPC(
            uint32_t predessor_idx,
            uint32_t child_idx,
            IAB::BidirectionalAxis axis,
            IAB::DescOfInharitance inharitance,
            uint32_t internal_max_tries = DEFAULT_MAX_TRIES
        ) noexcept;

        bool UnlinkTwoAPC(
            uint32_t child_idx,
            IAB::BidirectionalAxis axis,
            uint32_t internal_max_tries = DEFAULT_MAX_TRIES
        ) noexcept;
        

    };
    



}
