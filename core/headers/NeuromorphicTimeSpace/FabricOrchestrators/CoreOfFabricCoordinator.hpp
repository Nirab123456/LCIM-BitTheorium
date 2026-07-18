#pragma once 
#include "../../AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"

namespace PredictedAdaptedEncoding
{

    struct EnumsOfFabricCoordinator
    {
        /// UNCHECKED
        static constexpr size_t RELATION_WIDTH_OF_FABRIC = 8u;
        static constexpr size_t QUEUE_RECORD_WIDTH_OF_FABRIC = 2u;
        static constexpr size_t WORK_RECORD_WIDTH_OF_FABRIC = 4u;
        static constexpr size_t DEVICE_VIEW_WIDTH_OF_APC_FABRIC = 8u;
        static constexpr size_t THREAD_TABLE_RECORD_WIDTH = 4u;
        static constexpr size_t DEFAULT_THREAD_TABLE_CAPACITY = 256u;
        static constexpr size_t DEFAULT_FABRIC_CONTROLIO_LENGTH = 1024u;
        ///--------------------------


        enum class RecordBookInternalIndexing : uint8_t
        {
            BEGIN64 = 0,
            END64 = 1,
            UNASSIGNED_UNUSED_NANNULL = 2
        };
        static constexpr size_t RECORD_BOOK_WIDTH = static_cast<size_t>(RecordBookInternalIndexing::UNASSIGNED_UNUSED_NANNULL);

        enum class HashTableInternalIndexing : uint8_t
        {
            KEY_INDEX = 0,
            VALUE_INDEX = 1,
            PROB_DISTANCE_LOCK = 2,
            UNASSIGNED_UNUSED_NANNULL = 3
        };
        static constexpr size_t HASH_BUCKED_WIDTH_OF_FABRIC = static_cast<size_t>(HashTableInternalIndexing::UNASSIGNED_UNUSED_NANNULL);

        /// @brief DESCRIBS: Initial Fundamental Meta for An APC When Created 
        enum class DescriptionUnitIdentity : uint8_t
        {
            APC_INDEX = 0,
            APC_SEGMENTPOOL_BEGAIN_SLAB = 1,
            APC_SEGMENTPOOL_END_SLAB = 2,
            NEXT_APC_SAGMANTPOOL_BEGAIN = 3,
            LOGICAL_ID = 4,
            SHARED_GROUP_ID = 5,
            RELATION_HEADS = 6,
            RETIRE_EPOCH = 7,
            APC_FLAGS_FOR_THIS = 8,
            ID_STATE_CONCURRENT = 9,
            UNASSIGNED_UNUSED_NANNULL = 10
        };
        static constexpr size_t DESCRIPTION_WIDTH_AND_VALIDATION_IDX = static_cast<size_t>(DescriptionUnitIdentity::UNASSIGNED_UNUSED_NANNULL);



        enum class FabricMetaIndicies : uint8_t
        {
            MAGIC = 0,
            VERSION = 1,
            FLAGS = 2,
            SLAB_ID = 3,
            TOTAL_CELLS = 4,
            CONTROL_CELLS_OF_FABRIC = 5,
            APC_DESCRIPTION_COUNT = 6,
            PER_APC_RUNTIME_CELL_COUNT = 7,
            SEGMENT_POOL_BEGIN_IDX = 8,
            SEGMENT_POOL_END_IDX = 9,
            
            TOTAL_APC_IN_USE  = 10,
            RETIRE_SLOT_HEAD = 11,
            RELATION_FREE_HEAD = 12,
            RESERVED_13 = 13,
            RESERVED_14 = 14,
            NEXT_BRANCH_ID = 15,
            NEXT_RELATION_ID = 16,
            NEXT_DEVICE_VIEW_ID = 17,


            WORK_WRITE_CURSOR = 18,
            WORK_READ_CURSOR = 19,
            READY_WRITE_CURSOR = 20,
            READY_READ_CURSOR = 21,

            //COUNTS
            GLOBAL_EPOCH48 = 22,
            MIN_SAFE_EPOCH48 = 23,
            RELATION_RECLAIM_COUNT = 24,
            WORK_QUEUE_DROPPED_COUNT = 25,
            THREAD_ACTIVE_COUNT = 26,
            THREAD_REGISTRATION_FAILURE = 27,
            RELATION_TOMBSTONE_COUNT = 28,
            RELATION_UNLINK_FAILURES = 29,
            WORK_QUEUE_CLAIM_FAILURES = 30,
            CAS_FAILURE_COUNT = 31,
            ERROR_COUNT = 32,
            RETIRED_COUNT = 33,
            LIVE_SLOT_COUNT = 34,
            FABRIC_CLOCK16 = 35,
            ///end count
            
            RECORD_BOOK_OF_TSC_BEGIN = 36,
            RECORD_BOOK_OF_TSC_END = 37,
            TABLE_DIRECTORY_COUNT = 38,
            TABLE_DIRECTORY_VERSION = 39,


            HASH_TOMBSTONE_COUNT = 48,
            HASH_COMPACTION_COUNT = 49,
            WORK_QUEUE_OCCUPANCY = 50,
            READY_QUEUE_OCCUPANCY = 51,

            BACKOFF_SPIN_LIMIT = 52,
            BACKOFF_YIELD_LIMIT = 53,
            INITIALIZATION_STATE = 54,
            HAS_COMPACTION_INFLIGHT = 55,

            THREAD_TABLE_CAPACITY = 56,


            RESERVED_57_UPTO_94 = 57,

            EOF_FABRIC_HEADER = 95

        };


        enum class JustifyClaimCas
        {
            SUCCESS = 0,
            OUT_OF_BOUND = 1,
            INVALID_CELL = 2,
            CELL_SENTINAL_STATE = 3,
            CELL_INVALID = 4,
            CAS_LOOP_RANOUT = 5,
            UNDEFINED_CAS_FAILURE = 6,
            INVALID_USE_OF_METHOD = 7

        };

        /// @brief To Choose a Buffer to use 
        enum class SizeOfABuffer : uint8_t
        {
            SMALL = 0,
            MIDIUM = 1,
            LARGE = 3
        };
    };

    struct CoreOfFabricCoordinator : public EnumsOfFabricCoordinator
    {
        static constexpr uint32_t FABRIC_MAGIC = 0x41504643u;
        static constexpr uint32_t FABRIC_META_EOF = 0x41474946u;
        static constexpr uint8_t EACH_TABLE_RECORD_SENTINAL = UINT8_MAX;
        
        static constexpr uint32_t HASH32_GRATIO_1 = 2654435769u;
        static constexpr uint32_t HASH32_GRATIO_2 = 123456789u;

        static constexpr bool IsValidHashTable(FabricTableSegmentClasses table_class) noexcept
        {
            return table_class == FabricTableSegmentClasses::BRANCH_HASH ||
                table_class == FabricTableSegmentClasses::LOGICAL_HASH ||
                table_class == FabricTableSegmentClasses::SHARED_HASH;
        }

        static constexpr bool IsKnownDescriptionIdentity(DescriptionUnitIdentity identity_of_unit) noexcept
        {
            if (identity_of_unit < DescriptionUnitIdentity::UNASSIGNED_UNUSED_NANNULL)
            {
                return true;
            }
            return false;
        }
    
    };


    struct RawPackedCellAllocator
    {
        using AllocateFunction = uint64_t* (*)(
            size_t count_of_packed_cell, size_t alignment, void* user
        ) noexcept;

        using FreeFunction = void (*)(
            uint64_t* packed_cell_storage_ptr, 
            size_t count_of_cell, size_t alignment, void*user
        ) noexcept;

        AllocateFunction AllocatePackedCellStorage{nullptr};
        FreeFunction FreePackedCellStorage{nullptr};
        void* User{nullptr};
        size_t Alignment{BIT_LENGTH_OF_FABRIC};

        static size_t AlignBiteCount_(size_t bytes, size_t alignment) noexcept
        {
            if (alignment == UNSIGNED_ZERO)
            {
                return bytes;
            }

            const size_t remaining_bytes = bytes % alignment;
            return remaining_bytes == UNSIGNED_ZERO ? bytes : bytes + (alignment - remaining_bytes);
        }

        static uint64_t* DefaultAllocateAtomicCells(
            size_t count_of_packed_cell, size_t alignment, void*
        ) noexcept
        {
            if (count_of_packed_cell == UNSIGNED_ZERO)
            {
                return nullptr;
            }

            alignment = std::max<size_t>(alignment, alignof(uint64_t));
            const size_t byte_count = sizeof(uint64_t) * count_of_packed_cell;
            const size_t aligned_bytes = AlignBiteCount_(byte_count, alignment);

#if defined(_MSC_VER)

            void* raw_packed_cell_memory = _aligned_malloc(aligned_bytes, alignment);
#else
            void* raw_packed_cell_memory = std::aligned_alloc(alignment, aligned_bytes);
#endif
            if (!raw_packed_cell_memory)
            {
                return nullptr;
            }
            std::memset(raw_packed_cell_memory, UNSIGNED_ZERO, aligned_bytes);
            return static_cast<uint64_t*>(raw_packed_cell_memory);
            
        }

        static void DefaultFreeAtomicCells(
            uint64_t* packed_cell_storage_ptr, 
            size_t, size_t, void*
        ) noexcept
        {
            if (!packed_cell_storage_ptr)
            {
                return;
            }
#if defined(_MSC_VER)
            _aligned_free(packed_cell_storage_ptr);
#else
            std::free(packed_cell_storage_ptr);
#endif
        }
    };


}
