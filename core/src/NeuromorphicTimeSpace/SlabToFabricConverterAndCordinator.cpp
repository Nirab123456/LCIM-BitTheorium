#pragma once
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"
#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace BidirectionalInMemGraph
{


    uint64_t* SlabToFabricConverterAndCordinator::AllocatePackedCellRaw_(size_t count_of_cells) noexcept
    {
        auto allocation_function = AllocatorOfFabric_.AllocatePackedCellStorage ? 
            AllocatorOfFabric_.AllocatePackedCellStorage : &RawPackedCellAllocator::DefaultAllocateAtomicCells;
        
        size_t alignment = AllocatorOfFabric_.Alignment ? AllocatorOfFabric_.Alignment : BIT_LENGTH_OF_FABRIC;
        alignment = std::max<size_t>(alignment, alignof(uint64_t));
        alignment = std::max<size_t>(alignment, BIT_LENGTH_OF_FABRIC);

        return allocation_function(count_of_cells, alignment, AllocatorOfFabric_.User);

    }


    void SlabToFabricConverterAndCordinator::FreeRawPackedCells_(uint64_t* packed_cell_memory_ptr, size_t packed_cell_count) noexcept
    {
        RawPackedCellAllocator::FreeFunction free_function = AllocatorOfFabric_.FreePackedCellStorage ?
                            AllocatorOfFabric_.FreePackedCellStorage : &RawPackedCellAllocator::DefaultFreeAtomicCells;
        size_t alignment = AllocatorOfFabric_.Alignment ? AllocatorOfFabric_.Alignment : BIT_LENGTH_OF_FABRIC;
        alignment = std::max<size_t>(alignment, alignof(uint64_t));
        alignment = std::max<size_t>(alignment, BIT_LENGTH_OF_FABRIC);

        free_function(packed_cell_memory_ptr, packed_cell_count, alignment, AllocatorOfFabric_.User);
    }

    void SlabToFabricConverterAndCordinator::ResetScalarsofTheFabric_() noexcept
    {
        SlabBasePtr_ = nullptr;
        SlabCellCount_ = UNSIGNED_ZERO;
        PerAPCRuntimeCellCount_ = UNSIGNED_ZERO;
        CountOfAPC_ = UNSIGNED_ZERO;
        MaxDirectParentsPerAxis_ = UNSIGNED_ZERO;
        EdgeTableRecordWidth_ = UNSIGNED_ZERO;
        SegmentPoolBegin_ = CoreOfFabricCoordinator::FABRIC_UNIT_COUNT;
        FabricInitialized_.store(false, std::memory_order_release);
        InitializationInProgress_.store(false, std::memory_order_release);
    }



    void SlabToFabricConverterAndCordinator::InitializeCompleateFabricMetaIndices_(size_t record_book_begin, size_t record_book_end) noexcept
    {
        using FMI = CoreOfFabricCoordinator::FabricMetaIndicies;

        for (size_t i = 0; i < CoreOfFabricCoordinator::FABRIC_UNIT_COUNT; i++)
        {
            DirectlyStoreFabricUnit64(i, UNSIGNED_ZERO);
        }

        SlabBasePtr_[static_cast<size_t>(FMI::MAGIC)] = CoreOfFabricCoordinator::FABRIC_MAGIC;
        SlabBasePtr_[static_cast<size_t>(FMI::TOTAL_CELLS)] = SlabCellCount_;
        SlabBasePtr_[static_cast<size_t>(FMI::SEGMENT_POOL_BEGIN_IDX)] = SegmentPoolBegin_;
        SlabBasePtr_[static_cast<size_t>(FMI::PER_APC_RUNTIME_CELL_COUNT)] = PerAPCRuntimeCellCount_;
        SlabBasePtr_[static_cast<size_t>(FMI::RECORD_BOOK_OF_TSC_BEGIN)] = record_book_begin;
        SlabBasePtr_[static_cast<size_t>(FMI::RECORD_BOOK_OF_TSC_END)] = record_book_end;
        SlabBasePtr_[static_cast<size_t>(FMI::FIRST_FREE_IDX)] = UNSIGNED_ZERO;
        SlabBasePtr_[static_cast<size_t>(FMI::EDGE_TABLE_RECORD_WIDTH)] = EdgeTableRecordWidth_;
        SlabBasePtr_[static_cast<size_t>(FMI::MAX_DIRECT_PARENTS_PER_AXIS)] = MaxDirectParentsPerAxis_;
        SlabBasePtr_[static_cast<size_t>(FMI::EOF_FABRIC_HEADER)] = CoreOfFabricCoordinator::FABRIC_META_EOF;
    }


    bool SlabToFabricConverterAndCordinator::InitializeFabric(
        uint32_t slot_count,
        uint32_t slot_cell_count,
        uint8_t max_direct_parent_per_axis
    ) noexcept
    {
        bool expected = false;
        if (!InitializationInProgress_.compare_exchange_strong(expected, true, std::memory_order_acq_rel, std::memory_order_acquire))
        {
            return false;
        }

        struct InitGuardSTFC
        {
            SlabToFabricConverterAndCordinator* SelfPtr{};
            bool SuccesInit{false};


            ~InitGuardSTFC()
            {
                if (!SuccesInit && SelfPtr)
                {
                    SelfPtr->FabricInitialized_.store(false, std::memory_order_release);
                }
                
                if (SelfPtr)
                {
                    SelfPtr->InitializationInProgress_.store(false, std::memory_order_release);
                }
            }
        } internal_init_guard{this, false};
        
        ShutDownFabric();

        InitializationInProgress_.store(true, std::memory_order_release);

        if (slot_count == UNSIGNED_ZERO || !APCDataStructure::IsValid32BitAPCUnit(slot_count))
        {
            return false;
        }
        
        if (!APCDataStructure::IsCapacityOfAPCValid(slot_cell_count))
        {
            return false;
        }

        if (
            !EdgeBuilder::IsValidConfigurableParentCapacity(max_direct_parent_per_axis) ||
            slot_count > (uint32_t{1u} << EdgeBuilder::RELATION_SLOT_BITS)
        )
        {
            return false;
        }
        MaxDirectParentsPerAxis_ = max_direct_parent_per_axis;
        EdgeTableRecordWidth_ = static_cast<uint16_t>(EdgeBuilder::EdgeTableRecordWidth(MaxDirectParentsPerAxis_));
        
        CountOfAPC_ = static_cast<uint64_t>(slot_count);
        PerAPCRuntimeCellCount_ = static_cast<uint32_t>(slot_cell_count);

        size_t cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(CoreOfFabricCoordinator::FABRIC_UNIT_COUNT);
        const size_t record_book_begin = cursor;
        const size_t record_book_end = record_book_begin + static_cast<size_t>(RecordBookConf::RECORD_BOOK_INTERNAL_SEGMENT_COUNT) * CoreOfFabricCoordinator::RECORD_BOOK_WIDTH;

        cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(record_book_end);
        const size_t horizontal_edge_begin = cursor;
        const size_t horizontal_edge_end = horizontal_edge_begin + 
                static_cast<size_t>(CountOfAPC_) * EdgeTableRecordWidth_;
        
        cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(horizontal_edge_end);
        const size_t vertical_edge_begin = cursor;
        const size_t vertical_edge_end = vertical_edge_begin + 
                static_cast<size_t>(CountOfAPC_) * EdgeTableRecordWidth_;

        cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(vertical_edge_end);
        const size_t apc_handle_table_begin = cursor;
        const size_t apc_handle_table_end = apc_handle_table_begin + static_cast<size_t>(CountOfAPC_ * HandleOfAPCStatic::HANDLE_TABLE_WIDTH);

        cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(apc_handle_table_end);
        const size_t free_list_begin = cursor;
        const size_t free_list_end = free_list_begin + static_cast<size_t>(CountOfAPC_ * CoreOfFabricCoordinator::QUEUE_RECORD_WIDTH_OF_FABRIC);

        cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(free_list_end);
        const size_t ready_queue_begin = cursor;
        const size_t ready_queue_end = ready_queue_begin + static_cast<size_t>(CountOfAPC_ * CoreOfFabricCoordinator::QUEUE_RECORD_WIDTH_OF_FABRIC);

        cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(ready_queue_end);
        const size_t work_queue_begin = cursor;
        const size_t work_queue_end = work_queue_begin + static_cast<size_t>(CountOfAPC_ * CoreOfFabricCoordinator::WORK_RECORD_WIDTH_OF_FABRIC);

        cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(work_queue_end);
        const size_t device_view_table_begin = cursor;
        const size_t device_view_table_end = device_view_table_begin + static_cast<size_t>(CountOfAPC_ * CoreOfFabricCoordinator::DEVICE_VIEW_WIDTH_OF_APC_FABRIC);

        cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(device_view_table_end);
        SegmentPoolBegin_ = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(std::max<size_t>(cursor, CoreOfFabricCoordinator::DEFAULT_FABRIC_CONTROLIO_LENGTH));
        SlabCellCount_ = SegmentPoolBegin_ + static_cast<size_t>(CountOfAPC_ * PerAPCRuntimeCellCount_);

        if (SlabCellCount_ == UNSIGNED_ZERO || SlabCellCount_ >= FABRIC_CELL_SENTINAL)
        {
            return false;
        }
        SlabBasePtr_ = AllocatePackedCellRaw_(SlabCellCount_);
        if (!SlabBasePtr_)
        {
            return false;
        }

        for (size_t idx = 0; idx < SlabCellCount_; idx++)
        {
            DirectlyStoreFabricUnit64(idx, UNSIGNED_ZERO);
        }

        InitializeCompleateFabricMetaIndices_(record_book_begin, record_book_end);

        //RECORD_BOOK_OF_TABLE_SEGMENT_CLASS - ENTRIES
        WriteARecordBookOfTSCEntry_(FabricSegments::SLAB_RECORD_MAP, record_book_begin, record_book_end);
        WriteARecordBookOfTSCEntry_(FabricSegments::HORIZONTAL_EDGE_TABLE, horizontal_edge_begin, horizontal_edge_end);
        WriteARecordBookOfTSCEntry_(FabricSegments::VERTICAL_EDGE_TABLE, vertical_edge_begin, vertical_edge_end);
        WriteARecordBookOfTSCEntry_(FabricSegments::APC_HANDLE_TABLE, apc_handle_table_begin, apc_handle_table_end);
        WriteARecordBookOfTSCEntry_(FabricSegments::FREE_APC_LIST, free_list_begin, free_list_end);
        WriteARecordBookOfTSCEntry_(FabricSegments::READY_QUEUE, ready_queue_begin, ready_queue_end);
        WriteARecordBookOfTSCEntry_(FabricSegments::WORK_QUEUE, work_queue_begin, work_queue_end);
        WriteARecordBookOfTSCEntry_(FabricSegments::DEVICE_VIEW_TABLE, device_view_table_begin, device_view_table_end);
        WriteARecordBookOfTSCEntry_(FabricSegments::SEGMENT_POOL, SegmentPoolBegin_, SlabCellCount_);

        HorizontalEdgeBeginIdx_ = horizontal_edge_begin;
        VerticalEdgeBeginIdx_ = vertical_edge_begin;
        HandleTableBeginIndex_ = apc_handle_table_begin;

        if (!InitializeAPCGenerationTable_())
        {
            return false;
        }
        
        //IDLE UNUSED FabricSegments
        IdleAFabricTableClassRangesMemory_(FabricSegments::FREE_APC_LIST);
        IdleAFabricTableClassRangesMemory_(FabricSegments::READY_QUEUE);
        IdleAFabricTableClassRangesMemory_(FabricSegments::WORK_QUEUE);
        IdleAFabricTableClassRangesMemory_(FabricSegments::DEVICE_VIEW_TABLE);
        //END:: IDELING

        //INIT: EDGE TABLES
        if (
            !InitializeEdgeTable_(FabricSegments::HORIZONTAL_EDGE_TABLE) ||
            !InitializeEdgeTable_(FabricSegments::VERTICAL_EDGE_TABLE)
        )
        {
            return false;
        }
        //END::: 
        //INIT:Life Cycle
        InitAllAPCLifeCycleState();

        //CONFERMATION
        FabricInitialized_.store(true, std::memory_order_release);
        internal_init_guard.SuccesInit = true;
        return true;
        
    }
    
    void SlabToFabricConverterAndCordinator::ShutDownFabric() noexcept
    {

        FabricInitialized_.store(false, std::memory_order_release);
        uint64_t* old_ptr = SlabBasePtr_;
        const size_t old_count = SlabCellCount_;
        SlabBasePtr_ = nullptr;
        SlabCellCount_ = UNSIGNED_ZERO;
        if (old_ptr)
        {
            FreeRawPackedCells_(old_ptr, old_count);
        }
        ResetScalarsofTheFabric_();
    }


}
