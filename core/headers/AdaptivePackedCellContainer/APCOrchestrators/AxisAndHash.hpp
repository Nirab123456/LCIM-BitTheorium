
#pragma once 
#include <array>
#include <utility>
#include "APCDataStructure.hpp"
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

    /// @brief CREATS: HASH OrdinalKey: Based On a Desired SHARED / LOGICAL Group ID
    /// @param ordinal ORDINAL IDX < UINT32_MAX - 1
    /// @return IF INVALID: UINT64_MAX
    static constexpr uint64_t MakeGroupKeyFromParentGroupId(uint64_t group_id, uint32_t ordinal) noexcept
    {
        if (
            !IsValidGroupId(group_id) ||
            !APCDataStructure::IsValid32BitAPCUnit(ordinal)
        )
        {
            return FABRIC_CELL_SENTINAL;
        }
        return Double32In64ForAPCandFabric::PackDoubleUnsigned32In64(
            static_cast<uint32_t>(ordinal),
            static_cast<uint32_t>(group_id)
        );
    }


    static constexpr std::optional<uint32_t> GroupPreFix32FromKey(uint64_t group_key) noexcept
    {
        const std::optional<uint32_t> prefix_32 = Double32In64ForAPCandFabric::ExtractHigh32Of64(group_key);
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
        const std::optional<uint32_t> ordinal = Double32In64ForAPCandFabric::ExtractLow32Of64(group_key);
        if (
            !IsValidAPCId(group_key) ||
            !ordinal.has_value()||
            !APCDataStructure::IsValid32BitAPCUnit(ordinal.value())
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

    static constexpr uint64_t NextPowerOf2Unsigned64(uint64_t given_value) noexcept
    {
        if (given_value <= 2u)
        {
            return 2u;
        }
        --given_value;
        given_value |= given_value >> 1u;
        given_value |= given_value >> 2u;
        given_value |= given_value >> 4u;
        given_value |= given_value >> 8u;
        given_value |= given_value >> 16u;
        given_value |= given_value >> 32u;

        return given_value + 1u;
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
        HeaderIdentifierOfAPC PreviousSibling{HeaderIdentifierOfAPC::EOF_APC_HEADER};
        HeaderIdentifierOfAPC NextSibling{HeaderIdentifierOfAPC::EOF_APC_HEADER};
        HeaderIdentifierOfAPC OrdinalKey{HeaderIdentifierOfAPC::EOF_APC_HEADER};
        HeaderIdentifierOfAPC OwnRootKey{HeaderIdentifierOfAPC::EOF_APC_HEADER};
        HeaderIdentifierOfAPC RootOwnedChild{HeaderIdentifierOfAPC::EOF_APC_HEADER};
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
        uint64_t slot_handle,
        BidirectionalAxis axis
    ) noexcept
    {
        const uint64_t axis_salt = IsHorizontalSharedAxis(axis) ? HASH_64BIT_GRATIO_1 : HASH_64BIT_GRATIO_2;
        uint32_t group_id = static_cast<uint32_t>(
            HashUnsigned64(slot_handle ^ axis_salt)
        );
        if (!IsValidGroupId(group_id))
        {
            group_id ^= HASH_64BIT_GRATIO_3;
            group_id = IsValidAPCId(group_id) ? group_id : 1u;
        }
        return group_id;
    }


    static constexpr uint64_t ComposeNewGroupKey(
        uint64_t slot_handle,
        BidirectionalAxis axis,
        uint32_t ordinal
    ) noexcept
    {
        return MakeGroupKeyFromParentGroupId(
            DeriveGroupId(slot_handle, axis),
            ordinal
        );
    }

    static constexpr bool IsValidEven64(uint64_t value) noexcept
    {
        return APCDataStructure::IsValidFabricUnit(value) &&
            (value & 1u) == UNSIGNED_ZERO;
    }

};

struct DefineIdentityBuffer : public AxisConstructor
{
    static constexpr uint8_t IDENTITY_BUFFER_LEN = APCDataStructure::TotalIdentityUnitCount() + 1;
    static constexpr uint8_t IDENTITY_VALIDATION_IDX = IDENTITY_BUFFER_LEN - 1;
    static constexpr uint64_t VALIDATION_IDENTITY_MARK = 987987;

    static constexpr uint64_t IDENTY_FINGERPRINT_WRITE_LOCK = FABRIC_CELL_SENTINAL - 1u;
    static constexpr uint64_t IDENTITY_FINGERPRINT_CONSUMED = FABRIC_CELL_SENTINAL - 3u;

    enum class FingerprintHashState : uint8_t
    {
        WRITE_LOCK = 0,
        CONSUME_LOCK = 1,
        VALID = 2,
        INVALID = 3
    };

    using BufferOfAPCIdentity = std::array<uint64_t, IDENTITY_BUFFER_LEN>;

    static constexpr bool IsKnownIdentity(HeaderIdentifierOfAPC identity_unit) noexcept
    {
        return identity_unit >= HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT &&
            identity_unit <= HeaderIdentifierOfAPC::PREVIOUS_VERTICAL_SLOT;
    }

    static constexpr bool IsHoldFingerprintState(FingerprintHashState state) noexcept
    {
        return state == FingerprintHashState::WRITE_LOCK ||
            state == FingerprintHashState::CONSUME_LOCK;
    }


    static constexpr std::optional<uint8_t> GetBufferIdxFromIdentityUnit(HeaderIdentifierOfAPC identity_unit) noexcept
    {
        if (!IsKnownIdentity(identity_unit))
        {
            return std::nullopt;
        }
        
        return static_cast<uint8_t>(
            static_cast<uint8_t>(identity_unit) - static_cast<uint8_t>(HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT)
        );
    }

    static constexpr std::optional<HeaderIdentifierOfAPC> GetIdentityUnitFromBufferIdx(uint8_t buffer_idx) noexcept
    {
        if (
            buffer_idx >= APCDataStructure::TotalIdentityUnitCount()
        )
        {
            return std::nullopt;
        }
        return static_cast<HeaderIdentifierOfAPC>(
            buffer_idx + static_cast<uint8_t>(HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT)
        );
    }

        static constexpr FingerprintHashState StateOfIdentityFingerprint(uint64_t hash_value) noexcept
        {
            if (
                !IsValidAPCId(hash_value)
            )
            {
                return FingerprintHashState::INVALID;
            }

            if (hash_value == IDENTITY_FINGERPRINT_CONSUMED)
            {
                return FingerprintHashState::CONSUME_LOCK;
            }

            if (hash_value == IDENTY_FINGERPRINT_WRITE_LOCK)
            {
                return FingerprintHashState::WRITE_LOCK;
            }
            
            return FingerprintHashState::VALID;
        }

        static constexpr void BuildNullIdentityBuffer(BufferOfAPCIdentity& identity_buffer) noexcept
        {
            identity_buffer.fill(FABRIC_CELL_SENTINAL);
            identity_buffer[IDENTITY_VALIDATION_IDX] = UNSIGNED_ZERO;
        }

        static constexpr bool InsertAnIdentityInBuffer(
            BufferOfAPCIdentity& identity_buffer,
            HeaderIdentifierOfAPC identity,
            uint64_t value
        ) noexcept
        {
            const std::optional<uint8_t> buffer_idx = GetBufferIdxFromIdentityUnit(identity);
            if (!buffer_idx.has_value())
            {
                return false;
            }

            if (
                !APCDataStructure::IsValidFabricUnit(value) &&
                !(value == FABRIC_CELL_SENTINAL && CanIdentityContainRuntimeSentinal(identity))
            )
            {
                return false;
            }
            identity_buffer[buffer_idx.value()] = value;
            identity_buffer[IDENTITY_VALIDATION_IDX] = UNSIGNED_ZERO;
            return true;
        }

        static constexpr uint64_t ValueOfAnIdentityFromBuffer(
            const BufferOfAPCIdentity& identity_buffer,
            HeaderIdentifierOfAPC identity
        ) noexcept
        {
            const std::optional<uint8_t> buffer_idx = GetBufferIdxFromIdentityUnit(identity);
            if (!buffer_idx.has_value())
            {
                return FABRIC_CELL_SENTINAL;
            }

            return identity_buffer[buffer_idx.value()];
        }

        static constexpr bool CanIdentityContainRuntimeSentinal(HeaderIdentifierOfAPC identity) noexcept
        {

            return 
                identity == HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT ||
                identity == HeaderIdentifierOfAPC::VERTICAL_ORDINAL_KEY ||
                identity == HeaderIdentifierOfAPC::HORIZONTAL_ORDINAL_KEY ||
                identity == HeaderIdentifierOfAPC::HORIZONTAL_ROOT_KEY ||
                identity == HeaderIdentifierOfAPC::VERTICAL_ROOT_KEY ||
                identity == HeaderIdentifierOfAPC::PREVIOUS_HORIZONTAL_SLOT ||
                identity == HeaderIdentifierOfAPC::PREVIOUS_VERTICAL_SLOT ||
                identity == HeaderIdentifierOfAPC::NEXT_HORIZONTAL_SLOT ||
                identity == HeaderIdentifierOfAPC::NEXT_VERTICAL_SLOT ||
                identity == HeaderIdentifierOfAPC::HORIZONTAL_NEXT_OF_ROOT ||
                identity == HeaderIdentifierOfAPC::VERTICAL_NEXT_OF_ROOT;
        }


        static constexpr bool DoseIdentityBufferContainsValidationMarker(const BufferOfAPCIdentity& identity_buffer) noexcept
        {
            return identity_buffer[IDENTITY_VALIDATION_IDX] == VALIDATION_IDENTITY_MARK;
        }

        static constexpr uint64_t ComposeIdentityFingerprint(
            const BufferOfAPCIdentity& identity_buffer
        ) noexcept
        {
            uint64_t hash = HASH_64BIT_GRATIO_1;
            for (uint8_t i = 1u; i < APCDataStructure::TotalIdentityUnitCount(); ++i)
            {
                hash ^= identity_buffer[i] + HASH_64BIT_GRATIO_1 + (hash << 6u) + (hash >> 2u);
                hash = HashUnsigned64(hash);
            }

            hash &= ~uint64_t{1};

            const FingerprintHashState state_fp = StateOfIdentityFingerprint(hash);


            if (
                state_fp != FingerprintHashState::VALID
            )
            {
                return 2u;
            }
            return hash;
        }


};


}