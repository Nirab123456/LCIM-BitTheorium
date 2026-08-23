
#pragma once 
#include <array>
#include <utility>
#include "APCEnums.h"

namespace BidirectionalInMemGraph
{


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

        static constexpr uint8_t BuindesHeaderIndexForColumnName(MacroColumnOfAPC macro_column) noexcept
        {
            return static_cast<uint8_t>(HeaderIdentifierOfAPC::FEEDFORWARD_BOUNDS) + static_cast<uint8_t>(macro_column);
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