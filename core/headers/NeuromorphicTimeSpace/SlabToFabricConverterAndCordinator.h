#pragma once 
#include "FabricTableConstructors/CompleteFabric.h"

namespace PredictedAdaptedEncoding
{
    
    
    class SlabToFabricConverterAndCordinator : public DescriptionOfAPC
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
            uint8_t slab_id = APCDataStructure::BRANCH_VERSION,
            uint32_t fabric_thread_capacity = CoreOfFabricCoordinator::DEFAULT_THREAD_TABLE_CAPACITY
        ) noexcept;

        std::optional<uint64_t> NewApcFromFabric(
            APCAxisSelection desired_axis,
            IAB::BufferOfAPCIdentity& identity_buffer_new_apc
        ) noexcept;
        
    };



}
