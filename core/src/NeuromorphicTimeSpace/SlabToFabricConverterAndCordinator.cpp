#pragma once
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"
#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace PredictedAdaptedEncoding
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
        SlabId_ = APCDataStructure::BRANCH_VERSION;

        SegmentPoolBegin_ = APCDataStructure::METACELL_COUNT;
        SegmentPoolEnd_ = APCDataStructure::METACELL_COUNT;

        HashBucketCount_ = UNSIGNED_ZERO;
        RelationRecordCount_ = UNSIGNED_ZERO;
        DeviceViewRecordCount_ = UNSIGNED_ZERO;
        ThreadTableCapacity_  = UNSIGNED_ZERO;

        FabricInitialized_.store(false, std::memory_order_release);
        InitializationInProgress_.store(false, std::memory_order_release);
    }



    void SlabToFabricConverterAndCordinator::InitializeCompleateFabricMetaIndices_(size_t record_book_begin, size_t record_book_end) noexcept
    {
        using FMI = CoreOfFabricCoordinator::FabricMetaIndicies;

        for (size_t i = 0; i < APCDataStructure::METACELL_COUNT; i++)
        {
            DirectlyStoreFabricUnit64(i, UNSIGNED_ZERO);
        }

        SlabBasePtr_[static_cast<size_t>(FMI::MAGIC)] = CoreOfFabricCoordinator::FABRIC_MAGIC;
        SlabBasePtr_[static_cast<size_t>(FMI::SLAB_ID)] = SlabId_;
        SlabBasePtr_[static_cast<size_t>(FMI::TOTAL_CELLS)] = SlabCellCount_;
        SlabBasePtr_[static_cast<size_t>(FMI::APC_DESCRIPTION_COUNT)] = CountOfAPC_;
        SlabBasePtr_[static_cast<size_t>(FMI::SEGMENT_POOL_BEGIN_IDX)] = SegmentPoolBegin_;
        SlabBasePtr_[static_cast<size_t>(FMI::SEGMENT_POOL_END_IDX)] = SegmentPoolEnd_;
        SlabBasePtr_[static_cast<size_t>(FMI::RECORD_BOOK_OF_TSC_BEGIN)] = record_book_begin;
        SlabBasePtr_[static_cast<size_t>(FMI::RECORD_BOOK_OF_TSC_END)] = record_book_end;
        SlabBasePtr_[static_cast<size_t>(FMI::THREAD_TABLE_CAPACITY)] = ThreadTableCapacity_;
        SlabBasePtr_[static_cast<size_t>(FMI::EOF_FABRIC_HEADER)] = CoreOfFabricCoordinator::FABRIC_META_EOF;
    }


    void SlabToFabricConverterAndCordinator::InitializeAPCDescriptorTable_() noexcept
    {

        for (uint64_t desc_idx = 0; desc_idx < CountOfAPC_; desc_idx++)
        {
            const DescriptorConf::APCDescriptorRange self_range = ReadAPCDescriptionRanges(desc_idx);
            const APCSegmentPoolRange segment_pool_range = GetSegmentPoolBegainEndForSingleAPCDescription(desc_idx);
            if (!self_range.IsValid || !segment_pool_range.IsValid)
            {
                continue;
            }
            const APCSegmentPoolRange next_segment_pool_range = GetSegmentPoolBegainEndForSingleAPCDescription(desc_idx + 1);

            DescriptionOfAPC::SingleAPCDescriptionCellBuffer description_buffer{};

            bool dsc_ok = DescriptionOfAPC::ConstructInitialAPCDescriptionBuffer(
                description_buffer,
                desc_idx,
                segment_pool_range.BeginIndex,
                segment_pool_range.EndIndex,
                !next_segment_pool_range.IsValid ? UNSIGNED_ZERO : next_segment_pool_range.BeginIndex
            );

            if (!dsc_ok)
            {
                continue;
            }

            if (
                !ForceNxLenMemCopy(
                    self_range.BeginIndex,
                    CoreOfFabricCoordinator::DESCRIPTION_WIDTH_AND_VALIDATION_IDX,
                    description_buffer.data()
                )
            )
            {
                continue;
            }
            
            
            for (size_t seg_idx = segment_pool_range.BeginIndex; seg_idx < segment_pool_range.EndIndex; seg_idx++)
            {
                DirectlyStoreFabricUnit64(seg_idx, UNSIGNED_ZERO);
            }
            
        }
                
    }

    bool SlabToFabricConverterAndCordinator::InitializeFabric(
        uint16_t slot_count,
        size_t slot_cell_count,
        uint8_t slab_id,
        uint32_t fabric_thread_capacity
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

        if (slot_count == UNSIGNED_ZERO || !APCDataStructure::IsValidControlAPCUnit(slot_count))
        {
            return false;
        }
        
        if (slot_cell_count < MINIMUM_APC_CELL_COUNT)
        {
            slot_cell_count = MINIMUM_APC_CELL_COUNT;
        }

        CountOfAPC_ = static_cast<uint64_t>(slot_count);
        PerAPCRuntimeCellCount_ = static_cast<uint16_t>(slot_cell_count);
        SlabId_ = slab_id == UNSIGNED_ZERO ? APCDataStructure::BRANCH_VERSION : slab_id;
        ThreadTableCapacity_ = fabric_thread_capacity == UNSIGNED_ZERO ? CoreOfFabricCoordinator::DEFAULT_THREAD_TABLE_CAPACITY : fabric_thread_capacity;

        HashBucketCount_ = HashHelpers::BucketCountForExpectedEntries(CountOfAPC_);

        if (HashBucketCount_ == UNSIGNED_ZERO || HashBucketCount_ >= FABRIC_CELL_SENTINAL)
        {
            return false;
        }
        
        RelationRecordCount_ = HashHelpers::NextPowerOf2Unsigned64(std::max<uint64_t>(HashHelpers::MIN_LIMIT_POW_OF_2, CountOfAPC_ * HashHelpers::DEFAULT_TABLE_TAILROOM_MULT));
        DeviceViewRecordCount_ = HashHelpers::NextPowerOf2Unsigned64(std::max<uint64_t>(HashHelpers::MIN_LIMIT_POW_OF_2, CountOfAPC_ )); // NO EXTRA TAILROOM

        size_t cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(APCDataStructure::METACELL_COUNT);
        const size_t record_book_begin = cursor;
        const size_t record_book_end = record_book_begin + static_cast<size_t>(RecordBookConf::RECORD_BOOK_INTERNAL_SEGMENT_COUNT) * CoreOfFabricCoordinator::RECORD_BOOK_WIDTH;

        cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(record_book_end);
        const size_t apc_description_begin = cursor;
        const size_t apc_description_end = apc_description_begin + static_cast<size_t>(CountOfAPC_ * CoreOfFabricCoordinator::DESCRIPTION_WIDTH_AND_VALIDATION_IDX);

        cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(apc_description_end);
        const size_t branch_hash_begin = cursor;
        const size_t branch_hash_end = branch_hash_begin + static_cast<size_t>(HashBucketCount_ * CoreOfFabricCoordinator::HASH_BUCKED_WIDTH_OF_FABRIC);

        cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(branch_hash_end);
        const size_t logical_hash_begin = cursor;
        const size_t logical_hash_end = logical_hash_begin + static_cast<size_t>(HashBucketCount_ * CoreOfFabricCoordinator::HASH_BUCKED_WIDTH_OF_FABRIC);
        
        cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(logical_hash_end);
        const size_t shared_hash_begin = cursor;
        const size_t shared_hash_end = shared_hash_begin + static_cast<size_t>(HashBucketCount_ * CoreOfFabricCoordinator::HASH_BUCKED_WIDTH_OF_FABRIC);

        cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(shared_hash_end);
        const size_t edge_table_begin = cursor;
        const size_t edge_table_end = edge_table_begin + static_cast<size_t>(CountOfAPC_ * CoreOfFabricCoordinator::QUEUE_RECORD_WIDTH_OF_FABRIC);

        cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(edge_table_end);
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
        const size_t device_view_table_end = device_view_table_begin + static_cast<size_t>(DeviceViewRecordCount_ * CoreOfFabricCoordinator::DEVICE_VIEW_WIDTH_OF_APC_FABRIC);

        cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(device_view_table_end);
        const size_t thread_table_begin = cursor;
        const size_t thread_table_end = thread_table_begin + static_cast<size_t>(ThreadTableCapacity_ * CoreOfFabricCoordinator::THREAD_TABLE_RECORD_WIDTH);

        cursor = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(thread_table_end);
        SegmentPoolBegin_ = CoreOfFabricCoordinator::DefaultFabricAlignment16Cell_(std::max<size_t>(cursor, CoreOfFabricCoordinator::DEFAULT_FABRIC_CONTROLIO_LENGTH));
        SegmentPoolEnd_ = SegmentPoolBegin_ + static_cast<size_t>(CountOfAPC_ * PerAPCRuntimeCellCount_);
        SlabCellCount_ = SegmentPoolEnd_;

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
        WriteARecordBookOfTSCEntry_(FabricTableSegmentClasses::SLAB_RECORD_MAP, record_book_begin, record_book_end);
        WriteARecordBookOfTSCEntry_(FabricTableSegmentClasses::APC_HANDLE_DESCRIPTOR, apc_description_begin, apc_description_end);
        WriteARecordBookOfTSCEntry_(FabricTableSegmentClasses::BRANCH_HASH, branch_hash_begin, branch_hash_end);
        WriteARecordBookOfTSCEntry_(FabricTableSegmentClasses::LOGICAL_HASH, logical_hash_begin, logical_hash_end);
        WriteARecordBookOfTSCEntry_(FabricTableSegmentClasses::SHARED_HASH, shared_hash_begin, shared_hash_end);
        WriteARecordBookOfTSCEntry_(FabricTableSegmentClasses::EDGE_TABLE, edge_table_begin, edge_table_end);
        WriteARecordBookOfTSCEntry_(FabricTableSegmentClasses::FREE_APC_LIST, free_list_begin, free_list_end);
        WriteARecordBookOfTSCEntry_(FabricTableSegmentClasses::READY_QUEUE, ready_queue_begin, ready_queue_end);
        WriteARecordBookOfTSCEntry_(FabricTableSegmentClasses::WORK_QUEUE, work_queue_begin, work_queue_end);
        WriteARecordBookOfTSCEntry_(FabricTableSegmentClasses::DEVICE_VIEW_TABLE, device_view_table_begin, device_view_table_end);
        WriteARecordBookOfTSCEntry_(FabricTableSegmentClasses::THREAD_TABLE, thread_table_begin, thread_table_end);
        WriteARecordBookOfTSCEntry_(FabricTableSegmentClasses::SEGMENT_POOL, SegmentPoolBegin_, SegmentPoolEnd_);
        //ENTRIES:: END ::SLAB_RECORD_MAP

        //IDLE UNUSED FabricTableSegmentClasses
        IdleAFabricTableClassRangesMemory_(FabricTableSegmentClasses::EDGE_TABLE);
        IdleAFabricTableClassRangesMemory_(FabricTableSegmentClasses::FREE_APC_LIST);
        IdleAFabricTableClassRangesMemory_(FabricTableSegmentClasses::READY_QUEUE);
        IdleAFabricTableClassRangesMemory_(FabricTableSegmentClasses::WORK_QUEUE);
        IdleAFabricTableClassRangesMemory_(FabricTableSegmentClasses::DEVICE_VIEW_TABLE);
        IdleAFabricTableClassRangesMemory_(FabricTableSegmentClasses::THREAD_TABLE);
        //END:: IDELING

        //INIT: HASH TABLES
        InitializeHashTable_(FabricTableSegmentClasses::BRANCH_HASH);
        InitializeHashTable_(FabricTableSegmentClasses::LOGICAL_HASH);
        InitializeHashTable_(FabricTableSegmentClasses::SHARED_HASH);
        //END::: 
        //INIT:DESCRIPTOR TABLE
        InitializeAPCDescriptorTable_();

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


    bool SlabToFabricConverterAndCordinator::ResolveIDConfOfAPC(
        APCGroupReserver::APCInitialIdentityStruct& container_initial_conf
    ) noexcept
    {
        if (
            !APCGroupReserver::IsMinimalValidCreateRequestOfAPC(container_initial_conf) || 
            container_initial_conf.APCSlotIndex >= CountOfAPC_ 
        )
        {
            container_initial_conf.IsAssignable = false;
            return false;
        }

        const uint64_t handle = HashIdConstructror::APCSlotIdxToHashTableHandler(container_initial_conf.APCSlotIndex);
        if (!HashIdConstructror::IsValidHashHandle(handle))
        {
            container_initial_conf.IsAssignable = false;
            return false;
        }

        container_initial_conf.BranchID = MakeUniqueBranchIdForHashAndAPC();
        container_initial_conf.AccessPassword = HashIdConstructror::MakeARandomFabricValid64();
        if (
            !HashIdConstructror::IsValidAPCId(container_initial_conf.BranchID) ||
            !HashIdConstructror::IsValidAPCId(container_initial_conf.AccessPassword)
        )
        {
            container_initial_conf.IsAssignable = false;
            return false;
        }

        return true;
    }


    uint64_t SlabToFabricConverterAndCordinator::MakeUniqueBranchIdForHashAndAPC() noexcept
    {
        for (size_t i = 0; i < DEFAULT_MAX_TRIES; i++)
        {
            const uint64_t random_bid = HashIdConstructror::MakeARandomFabricValid64();
            if (
                HashIdConstructror::IsValidAPCId(random_bid) && 
                !FindUsedHashValue(FabricTableSegmentClasses::BRANCH_HASH, random_bid).has_value()
            )
            {
                return random_bid;
            }
        }
        return FABRIC_CELL_SENTINAL;
    }


}
