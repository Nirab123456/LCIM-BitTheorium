
#pragma once 
#include <array>
#include <utility>
#include "../../SharedComponents/SharedConf.hpp"

namespace BidirectionalInMemGraph
{
    enum class HeaderIdentifierOfAPC : uint8_t
    {
        // identity
        MAGIC_ID = 0,

        GRAPH_MUTATION_AND_LOCK = 1,
        APC_SLOT_IDX = 2,
        BOUNDS_BEGIN = 3,
        BOUNDS_END = 4,
        HORIZONTAL_SHARED_IDX = 5,
        VERTICAL_SHARED_IDX = 6,
        HORIZONTAL_ROOT_IDX = 7,
        VERTICAL_ROOT_IDX = 8,
        HORIZONTAL_NEXT_OF_ROOT = 9,
        VERTICAL_NEXT_OF_ROOT = 10,
        NEXT_HORIZONTAL_SLOT = 11,
        NEXT_VERTICAL_SLOT = 12,
        PREVIOUS_HORIZONTAL_SLOT = 13,
        PREVIOUS_VERTICAL_SLOT = 14,

        // payload bounds versions
        FEEDFORWARD_BOUNDS = 15,
        FEEDBACKWARD_BOUNDS = 16,
        LATERAL_BOUNDS = 17,
        STATE_BOUNDS = 18,
        ERROR_BOUNDS = 19,
        WEIGHTLESS_BOUNDS = 20,
        WEIGHT_BOUNDS= 21,
        AUX_BOUNDS = 22,
        HETEROGENOUS_PTR_BOUNDS = 23,
        FREE_BOUNDS = 24,
        // Self Record
        BRANCH_PRIORITY = 25,
        LAYOUT_VERSION = 26,
        LOCAL_FULL_CLOCK = 27,
        //ENQUEUE
        FEEDFORWARD_ENQUEUE_POSITION = 28,
        FEEDBACKWARD_ENQUEUE_POSITION = 29,
        LATERAL_ENQUEUE_POSITION = 30,
        STATE_ENQUEUE_POSITION = 31,
        ERROR_ENQUEUE_POSITION = 32,
        WEIGHTLESS_ENQUEUE_POSITION = 33,
        WEIGHT_ENQUEUE_POSITION = 34,
        AUX_ENQUEUE_POSITION = 35,
        HETEROGENOUS_ENQUEUE_POSITION = 36,
        FREE_ENQUEUE_POSITION = 37,
        //DEQUEUE
        FEEDFORWARD_DEQUEUE_POSITION = 38,
        FEEDBACKWARD_DEQUEUE_POSITION = 39,
        LATERAL_DEQUEUE_POSITION = 40,
        STATE_DEQUEUE_POSITION = 41,
        ERROR_DEQUEUE_POSITION = 42,
        WEIGHTLESS_DEQUEUE_POSITION = 43,
        WEIGHT_DEQUEUE_POSITION = 44,
        AUX_DEQUEUE_POSITION = 45,
        HETEROGENOUS_DEQUEUE_POSITION = 46,
        FREE_DEQUEUE_POSITION = 47,

        // Region schema: record width + protocol + format/version.
        FEEDFORWARD_REGION_SCHEMA = 48,
        FEEDBACKWARD_REGION_SCHEMA = 49,
        LATERAL_REGION_SCHEMA = 50,
        STATE_REGION_SCHEMA = 51,
        ERROR_REGION_SCHEMA = 52,
        WEIGHTLESS_REGION_SCHEMA = 53,
        WEIGHT_REGION_SCHEMA = 54,
        AUX_REGION_SCHEMA = 55,
        HETEROGENOUS_REGION_SCHEMA = 56,
        FREE_REGION_SCHEMA = 57,
        CURRENT_ACTIVE_THREADS = 58,
        APC_SCHEMA_ID = 59,

        APC_LIFE_CYCLE = 62,
        EOF_APC_HEADER = 63
    };

    static_assert(
        (static_cast<uint8_t>(HeaderIdentifierOfAPC::FREE_BOUNDS) - static_cast<uint8_t>(HeaderIdentifierOfAPC::FEEDFORWARD_BOUNDS)) ==
        (static_cast<uint8_t>(MacroColumnOfAPC::FREE_SLOT) - static_cast<uint8_t>(MacroColumnOfAPC::FEEDFORWARD_MESSAGE))
    );

    static_assert(
        (static_cast<uint8_t>(HeaderIdentifierOfAPC::FREE_ENQUEUE_POSITION) - static_cast<uint8_t>(HeaderIdentifierOfAPC::FEEDFORWARD_ENQUEUE_POSITION)) ==
        (static_cast<uint8_t>(MacroColumnOfAPC::FREE_SLOT) - static_cast<uint8_t>(MacroColumnOfAPC::FEEDFORWARD_MESSAGE))
    );

    struct RangeOfAPC
    {
        size_t BeginIndex = UNSIGNED_ZERO;
        size_t EndIndex = UNSIGNED_ZERO;
        bool IsValid = false;
    };

    struct LayoutHeaderIdentityOrchestrator
    {

        static constexpr uint8_t LayoutBufferBegainInMetaIndecies() noexcept
        {
            return static_cast<uint8_t>(HeaderIdentifierOfAPC::FEEDFORWARD_BOUNDS);
        }

        static constexpr uint8_t LayoutBufferEndInMetaIndecies() noexcept
        {
            return static_cast<uint8_t>(HeaderIdentifierOfAPC::FREE_BOUNDS);
        }

    };

    struct ColumnConf : public LayoutHeaderIdentityOrchestrator
    {
    public:

        static constexpr HeaderIdentifierOfAPC EnqueueHeaderIndexFromColumnName(MacroColumnOfAPC macro_column) noexcept
        {
            return static_cast<HeaderIdentifierOfAPC>(
                static_cast<uint8_t>(HeaderIdentifierOfAPC::FEEDFORWARD_ENQUEUE_POSITION) + static_cast<uint8_t>(macro_column)
            );
        }

        static constexpr HeaderIdentifierOfAPC DequeueHeaderIndexFromColumnName(MacroColumnOfAPC macro_column) noexcept
        {
            return static_cast<HeaderIdentifierOfAPC>(
                static_cast<uint8_t>(HeaderIdentifierOfAPC::FEEDFORWARD_DEQUEUE_POSITION) + static_cast<uint8_t>(macro_column)
            );
        }

        static constexpr HeaderIdentifierOfAPC SchemaHeaderIndexFromColumnName(MacroColumnOfAPC macro_column) noexcept
        {
            return static_cast<HeaderIdentifierOfAPC>(
                static_cast<uint8_t>(HeaderIdentifierOfAPC::FEEDFORWARD_REGION_SCHEMA) + static_cast<uint8_t>(macro_column)
            );
        }

        static constexpr uint8_t CountOfMacroColumn() noexcept
        {
            return static_cast<uint8_t>(MacroColumnOfAPC::FREE_SLOT) - static_cast<uint8_t>(MacroColumnOfAPC::FEEDFORWARD_MESSAGE) + 1;
        }

        static constexpr uint8_t BoundsIdxInHeader(MacroColumnOfAPC macro_column) noexcept
        {
            return static_cast<uint8_t>(HeaderIdentifierOfAPC::FEEDFORWARD_BOUNDS) + static_cast<uint8_t>(macro_column);
        }

        static constexpr HeaderIdentifierOfAPC BoundsMetaIdxInHeader(MacroColumnOfAPC macro_column) noexcept
        {
            return static_cast<HeaderIdentifierOfAPC>(
                static_cast<uint8_t>(HeaderIdentifierOfAPC::FEEDFORWARD_BOUNDS) + static_cast<uint8_t>(macro_column)
            );
        }
    };

    struct APCDataStructure : public ColumnConf
    {

        static constexpr uint8_t METACELL_COUNT = 64;
        static constexpr uint8_t FABRIC_CELL_COUNT = 64;


        static constexpr uint32_t BRANCH_MAGIC = 0x41504342u;//big-endian
        static constexpr uint32_t EOF_HEADER = 0x72616600;//big-endian
        static constexpr uint8_t BRANCH_VERSION = 1u;
        static constexpr uint32_t APC_INDEX_BOUND_SENTINAL = UINT32_MAX;
        // static constexpr uint32_t APC_ALL_INDEX_LIMIT = APC_INDEX_BOUND_SENTINAL - 1;
        static constexpr size_t APC_CACHELINE_SIZE = 64u;

        static constexpr uint64_t HASH_64BIT_GRATIO_1 = 0x9E3779B97F4A7C15ull;
        static constexpr uint64_t HASH_64BIT_GRATIO_2 = 0xD6E8FEB86659FD93ull;



        static constexpr bool IsValid32BitAPCUnit(uint64_t index) noexcept
        {
            return index < APC_INDEX_BOUND_SENTINAL;
        }

        static constexpr bool IsValidFabricUnit(uint64_t index) noexcept
        {
            return index < FABRIC_CELL_SENTINAL;
        }

        static constexpr bool InLimitOfUint8(uint32_t version) noexcept
        {
            return version < UINT8_MAX &&
                version > UNSIGNED_ZERO;
        }

        static constexpr bool IsCapacityOfAPCValid(uint64_t capacity) noexcept
        {
            return capacity >= MINIMUM_APC_CELL_COUNT &&
                IsValid32BitAPCUnit(capacity);
        }

        static constexpr bool IsPowerOfTwoValue(uint64_t value) noexcept
        {
            return value != UNSIGNED_ZERO && (value & (value - 1u)) == UNSIGNED_ZERO;
        }

        static constexpr uint8_t TotalIdentityUnitCount() noexcept
        {
            return static_cast<uint8_t>(HeaderIdentifierOfAPC::PREVIOUS_VERTICAL_SLOT) - 
                static_cast<uint8_t>(HeaderIdentifierOfAPC::GRAPH_MUTATION_AND_LOCK) + 1;
        }

        static uint64_t MakeARandomFabricValid64() noexcept
        {
            static std::atomic<uint64_t> global_counter{1u};

            auto SplitMix64 = [](uint64_t x) noexcept -> uint64_t
            {
                x += HASH_64BIT_GRATIO_1;
                x = (x ^ (x >> 30u)) * 0xBF58476D1CE4E5B9ull;
                x = (x ^ (x >> 27u)) * 0x94D049BB133111EBull;
                x = x ^ (x >> 31u);
                return x;
            };

            uint64_t random_seed = global_counter.fetch_add(1, std::memory_order_acq_rel);

            random_seed ^= static_cast<uint64_t>(
                std::chrono::high_resolution_clock::now().time_since_epoch().count()
            );

            random_seed ^= reinterpret_cast<uint64_t>(&random_seed);

            try
            {
                std::random_device random_device;
                random_seed ^= static_cast<uint64_t>(random_device());
            }
            catch(...)
            {
                random_seed ^= HASH_64BIT_GRATIO_2;
            }

            for (uint32_t attempt = 0; attempt < 8u; attempt++)
            {
                random_seed = SplitMix64(random_seed);

                if (
                    IsValidFabricUnit(random_seed) &&
                    random_seed > UNSIGNED_ZERO
                )
                {
                    return random_seed;
                }
            }
            
            const uint64_t fallback = SplitMix64(global_counter.fetch_add(1u, std::memory_order_acq_rel));

            return (IsValidFabricUnit(fallback) && fallback > UNSIGNED_ZERO) ? 
                fallback : FABRIC_CELL_SENTINAL;

        }


    protected:
            static constexpr void FreeAlignedRawPackedCells_(uint64_t* backing_ptr) noexcept
            {
                if (!backing_ptr)
                {
                    return;
                }
                ::operator delete[](static_cast<void*>(backing_ptr), std::align_val_t{APC_CACHELINE_SIZE});
            }
    };
}