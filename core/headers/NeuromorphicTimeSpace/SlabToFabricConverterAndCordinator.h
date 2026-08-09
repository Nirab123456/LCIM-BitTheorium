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
            uint8_t slab_id = APCDataStructure::BRANCH_VERSION,
            uint32_t fabric_thread_capacity = CoreOfFabricCoordinator::DEFAULT_THREAD_TABLE_CAPACITY
        ) noexcept;
        
    };

    class ConstructAPC : public SlabToFabricConverterAndCordinator
    {
    private:

        std::optional<DSA::StateOfAPC> ReadValidAPCRangeInternally__(
            uint32_t apc_slot_idx,
            DSA::InternalAPCRange& range_return
        ) noexcept;


        // std::optional<uint64_t> SwitchIdentityState__(
        //     IAB::GraphMutationState desired_state,
        //     uint32_t apc_slot_idx,
        //     std::optional<IAB::GraphMutationState> required_state,
        //     uint32_t max_tries = DEFAULT_MAX_TRIES
        // ) noexcept;

    public:
        std::optional<DSA::StateOfAPC> ReadIdentityBufferOfAPC(
            uint32_t apc_slot,
            IAB::BufferOfAPCIdentity& identity
        ) noexcept;

        bool ReadGraphMutationFlags(
            uint32_t slot_idx,
            IAB::GraphMutationValues& values
        ) noexcept;
        
        std::optional<uint64_t> NewApcFromFabric(
            IAB::BidirectionalAxis desired_axis,
            IAB::BufferOfAPCIdentity& identity_buffer_new_apc,
            bool wants_both_axis = false
        ) noexcept;

        bool AttachAPC(
            uint64_t root_apc_idx,
            uint64_t current_apc_idx,
            IAB::BidirectionalAxis axis
        ) noexcept;

        bool DetachAPC(
            uint64_t current_apc_idx,
            IAB::BidirectionalAxis axis
        ) noexcept;

        std::optional<uint64_t>CreateAPCInternal_(
            uint64_t apc_idx,
            bool wants_horizontal_root,
            bool wants_vertical_root,
            const LBO::
                LayoutSpanAndPercentageCarrier&
                    layout,
            const SD::
                InitialRegionalDtypeConf&
                    dtype,
            const SD::
                InitialRegionalProtocol&
                    protocol,
            uint8_t version
        ) noexcept;
    };
    



}
