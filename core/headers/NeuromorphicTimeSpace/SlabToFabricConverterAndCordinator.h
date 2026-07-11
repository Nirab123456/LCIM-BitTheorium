#pragma once 
#include "FabricTableConstructors/HashTablesConstructor.h"

namespace PredictedAdaptedEncoding
{
    
    
    class SlabToFabricConverterAndCordinator : public HashTablesConstructor
    {
    private:

        uint64_t* AllocatePackedCellRaw_(size_t count_of_cells) noexcept;
        
        void FreeRawPackedCells_(uint64_t*packed_cell_memory_ptr, size_t packed_cell_count) noexcept;

        void ResetScalarsofTheFabric_() noexcept;

        void InitializeHashTable_(FabricTableSegmentClasses table_class) noexcept;

        /// @brief CALLES: 4xUpdateValidPairedOccupancyApproxAtomically_ ON: Each LocalityPolicy
        void Zero4LocalityBasedOccupancyOfFabric_() noexcept;

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
            uint16_t slot_count,
            size_t slot_cell_count = MINIMUM_APC_CAPACITY,
            uint8_t slab_id = APCDataStructure::BRANCH_VERSION,
            uint32_t fabric_thread_capacity = DEFAULT_THREAD_TABLE_CAPACITY
        ) noexcept;

        bool ResolveIDConfOfAPC(
            APCGroupReserver::APCInitialIdentityStruct& a_initial_acp_conf
        ) noexcept;

        uint64_t MakeUniqueBranchIdForHashAndAPC() noexcept;
        
    };

    

}
