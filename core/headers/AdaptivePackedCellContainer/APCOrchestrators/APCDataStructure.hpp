
#pragma once 
#include <array>
#include <utility>
#include "../../SharedComponents/SharedConf.hpp"

namespace BidirectionalInMemGraph
{

    enum class HeaderIdentifierOfAPC : uint8_t
    {
        // Identity
        MAGIC_ID = 0,

        // Payload bounds versions
        FEEDFORWARD_BOUNDS       = 1,
        FEEDBACKWARD_BOUNDS      = 2,
        LATERAL_BOUNDS           = 3,
        STATE_BOUNDS             = 4,
        ERROR_BOUNDS             = 5,
        WEIGHTLESS_BOUNDS        = 6,
        WEIGHT_BOUNDS            = 7,
        AUX_BOUNDS               = 8,
        HETEROGENOUS_PTR_BOUNDS  = 9,
        FREE_BOUNDS              = 10,

        // Self Record
        RESERVED          = 11,
        RESERVED_1           = 12,
        RESERVED_2         = 13,

        // ENQUEUE
        FEEDFORWARD_ENQUEUE_POSITION      = 14,
        FEEDBACKWARD_ENQUEUE_POSITION      = 15,
        LATERAL_ENQUEUE_POSITION           = 16,
        STATE_ENQUEUE_POSITION             = 17,
        ERROR_ENQUEUE_POSITION             = 18,
        WEIGHTLESS_ENQUEUE_POSITION        = 19,
        WEIGHT_ENQUEUE_POSITION            = 20,
        AUX_ENQUEUE_POSITION               = 21,
        HETEROGENOUS_ENQUEUE_POSITION      = 22,
        FREE_ENQUEUE_POSITION              = 23,

        // DEQUEUE
        FEEDFORWARD_DEQUEUE_POSITION       = 24,
        FEEDBACKWARD_DEQUEUE_POSITION      = 25,
        LATERAL_DEQUEUE_POSITION           = 26,
        STATE_DEQUEUE_POSITION             = 27,
        ERROR_DEQUEUE_POSITION             = 28,
        WEIGHTLESS_DEQUEUE_POSITION        = 29,
        WEIGHT_DEQUEUE_POSITION            = 30,
        AUX_DEQUEUE_POSITION               = 31,
        HETEROGENOUS_DEQUEUE_POSITION      = 32,
        FREE_DEQUEUE_POSITION              = 33,

        // Region schema: record width + protocol + format/version.
        FEEDFORWARD_REGION_SCHEMA          = 34,
        FEEDBACKWARD_REGION_SCHEMA          = 35,
        LATERAL_REGION_SCHEMA              = 36,
        STATE_REGION_SCHEMA                = 37,
        ERROR_REGION_SCHEMA                = 38,
        WEIGHTLESS_REGION_SCHEMA            = 39,
        WEIGHT_REGION_SCHEMA                = 40,
        AUX_REGION_SCHEMA                   = 41,
        HETEROGENOUS_REGION_SCHEMA          = 42,
        FREE_REGION_SCHEMA                  = 43,

        RESERVED_4             = 44,
        APC_SCHEMA_ID                       = 45,

        APC_LIFE_CYCLE                      = 46,
        EOF_APC_HEADER                      = 47
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

        static constexpr uint8_t METACELL_COUNT = static_cast<uint8_t>(HeaderIdentifierOfAPC::EOF_APC_HEADER) + 1u;
        static constexpr uint8_t FABRIC_CELL_COUNT = 16;


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

        static constexpr bool IsValidEven64(uint64_t value) noexcept
        {
            return 
                (value & 1u) == UNSIGNED_ZERO;
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