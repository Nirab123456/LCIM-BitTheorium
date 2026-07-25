
#pragma once 
#include <array>
#include <utility>
#include "ConstructorsAndCarriersOfAPC.hpp"
#include "../../SharedComponents/BitPackers/ConAndCaDependentPacker.hpp"

namespace PredictedAdaptedEncoding
{


struct HashIdConstructror
{
    static constexpr uint64_t GROUP_IDX_BIT_BOUNDRY = 32u;
    static constexpr uint64_t GROUP_SEQUENTIAL_INDEX_MASK = UINT32_MAX;
    static constexpr uint64_t GROUP_PREFIX_MASK = UINT32_MAX;

    static constexpr uint64_t HASH_64BIT_GRATIO_1 = 0x9E3779B97F4A7C15ull;
    static constexpr uint64_t HASH_64BIT_GRATIO_2 = 0xD6E8FEB86659FD93ull;
    static constexpr uint64_t HASH_64BIT_GRATIO_3 = 0xbf58476d1ce4e5b9ull;
    static constexpr uint64_t HASH_64BIT_GRATIO_4 = 0x94d049bb133111ebull;

    static constexpr uint8_t HASH_SHIFT_FOR_64_C1 = 30u;
    static constexpr uint8_t HASH_SHIFT_FOR_64_C2 = 27u;
    static constexpr uint8_t HASH_SHIFT_FOR_64_C3 = 31u;


    /// @brief VALIDATES THE RAW ID 
    static constexpr bool IsValidAPCId(uint64_t value) noexcept
    {
        return value > UNSIGNED_ZERO && value < FABRIC_CELL_SENTINAL;
    }

    static constexpr bool IsValidGroupId(uint64_t value) noexcept
    {
        return value > UNSIGNED_ZERO && 
            APCDataStructure::IsValid32BitAPCUnit(value);
    }

    static constexpr bool IsValidAPCSlotIdx(uint64_t slot_idx) noexcept
    {
        return slot_idx < FABRIC_CELL_SENTINAL - 1;
    }

    static constexpr uint64_t APCSlotIdxToHashTableHandler(uint64_t apc_slot_idx) noexcept
    {
        if (IsValidAPCSlotIdx(apc_slot_idx))
        {
            return apc_slot_idx + 1;
        }
        return FABRIC_CELL_SENTINAL;
    }

    static constexpr uint64_t HashTableHandlerToAPCSlotIdx(uint64_t handler) noexcept
    {
        if (IsValidAPCId(handler))
        {
            return handler - 1;
        }
        return FABRIC_CELL_SENTINAL;
    }

    /// @brief CREATS: HASH KeyAndID: Based On a Desired SHARED / LOGICAL Group ID
    /// @param ordinal ORDINAL IDX < UINT32_MAX - 1
    /// @return IF INVALID: UINT64_MAX
    static constexpr uint64_t MakeGroupKeyFromParentGroupId(uint64_t group_id, uint32_t ordinal) noexcept
    {
        if (!IsValidGroupId(group_id))
        {
            return FABRIC_CELL_SENTINAL;
        }
        return Double32In64ExPa::PackDoubleUnsigned32In64(
            static_cast<uint32_t>(ordinal),
            static_cast<uint32_t>(group_id)
        );
    }


    static constexpr std::optional<uint32_t> GroupPreFix32FromKey(uint64_t group_key) noexcept
    {
        const std::optional<uint32_t> prefix_32 = Double32In64ExPa::ExtractHigh32Of64(group_key);
        if (
            !IsValidAPCId(group_key) ||
            !prefix_32.has_value()||
            !IsValidGroupId(prefix_32.value())
        )
        {
            return std::nullopt;
        }
        
        return prefix_32;
    }


    static constexpr std::optional<uint32_t> GetOrdinalFromKey(uint64_t group_key) noexcept
    {
        const std::optional<uint32_t> ordinal = Double32In64ExPa::ExtractLow32Of64(group_key);
        if (
            !IsValidAPCId(group_key) ||
            !ordinal.has_value()||
            !IsValidGroupId(ordinal.value())
        )
        {
            return std::nullopt;
        }
        
        return ordinal;
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

            if (IsValidAPCId(random_seed))
            {
                return random_seed;
            }
        }
        
        const uint64_t fallback = SplitMix64(global_counter.fetch_add(1u, std::memory_order_acq_rel));

        return IsValidAPCId(fallback) ? fallback : FABRIC_CELL_SENTINAL;

    }


    static constexpr uint64_t HashUnsigned64(uint64_t given_value) noexcept
    {
        given_value ^= given_value >> HASH_SHIFT_FOR_64_C1;
        given_value *= HASH_64BIT_GRATIO_3;
        given_value ^= given_value >> HASH_SHIFT_FOR_64_C2;
        given_value *= HASH_64BIT_GRATIO_4;
        given_value ^=  given_value >> HASH_SHIFT_FOR_64_C3;

        if (!APCDataStructure::IsValidFabricUnit(given_value))
        {
            return given_value - 1u;
        }

        if (given_value == UNSIGNED_ZERO)
        {
            return 1u;
        }
        return given_value;
    }

};

struct AxisConstructor : public HashIdConstructror
{
    enum class BidirectionalAxis : uint8_t
    {
        HORIZONTALLY_SHARED = 1,
        VARTICAL_LOGICAL = 2
    };

    struct AxisConstructionMap
    {
        FabricTableSegmentClasses HashTable{FabricTableSegmentClasses::NULLNAN};
        HeaderIdentifierOfAPC PreviousTarget{HeaderIdentifierOfAPC::EOF_APC_HEADER};
        HeaderIdentifierOfAPC NextTarget{HeaderIdentifierOfAPC::EOF_APC_HEADER};
        HeaderIdentifierOfAPC KeyAndID{HeaderIdentifierOfAPC::EOF_APC_HEADER};
        HeaderIdentifierOfAPC OwnRootKey{HeaderIdentifierOfAPC::EOF_APC_HEADER};
        HeaderIdentifierOfAPC RootNext{HeaderIdentifierOfAPC::EOF_APC_HEADER};
    };
    static_assert(sizeof(AxisConstructionMap) <= sizeof(uint64_t));

    static constexpr AxisConstructionMap ConstructAxisMap(BidirectionalAxis desired_axis) noexcept
    {
        if (desired_axis == BidirectionalAxis::HORIZONTALLY_SHARED)
        {
            return AxisConstructionMap{
                FabricTableSegmentClasses::HORIZONTAL_HASH,
                HeaderIdentifierOfAPC::PREVIOUS_HORIZONTAL_SLOT,
                HeaderIdentifierOfAPC::NEXT_HORIZONTAL_SLOT,
                HeaderIdentifierOfAPC::HORIZONTAL_ORDINAL_KEY,
                HeaderIdentifierOfAPC::HORIZONTAL_ROOT_KEY,
                HeaderIdentifierOfAPC::HORIZONTAL_NEXT_OF_ROOT
            };
        }

        return AxisConstructionMap{
            FabricTableSegmentClasses::VERTICAL_HASH,
            HeaderIdentifierOfAPC::PREVIOUS_VERTICAL_SLOT,
            HeaderIdentifierOfAPC::NEXT_VERTICAL_SLOT,
            HeaderIdentifierOfAPC::VERTICAL_ORDINAL_KEY,
            HeaderIdentifierOfAPC::VERTICAL_ROOT_KEY,
            HeaderIdentifierOfAPC::VERTICAL_NEXT_OF_ROOT

        };
    }


    static constexpr bool IsHorizontalSharedAxis(BidirectionalAxis desired_axis) noexcept
    {
        if (desired_axis == BidirectionalAxis::HORIZONTALLY_SHARED)
        {
            return true;
        }
        return false;
    }

    static constexpr uint32_t DeriveGroupId(
        uint64_t branch_id,
        BidirectionalAxis axis
    ) noexcept
    {
        const uint64_t axis_salt = IsHorizontalSharedAxis(axis) ? HASH_64BIT_GRATIO_1 : HASH_64BIT_GRATIO_2;
        uint32_t group_id = static_cast<uint32_t>(
            HashUnsigned64(branch_id ^ axis_salt)
        );
        if (!IsValidGroupId(group_id))
        {
            group_id ^= HASH_64BIT_GRATIO_3;
            group_id = IsValidAPCId(group_id) ? group_id : 1u;
        }
        return group_id;
    }


    static constexpr uint64_t HashGroupId(
        uint64_t branch_id,
        BidirectionalAxis axis,
        uint32_t ordinal
    ) noexcept
    {
        return MakeGroupKeyFromParentGroupId(
            DeriveGroupId(branch_id, axis),
            ordinal
        );
    }

};

}