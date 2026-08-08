
#pragma once 
#include <array>
#include <utility>
#include "APCDataStructure.hpp"
#include "../../SharedComponents/BitPackers/ConAndCaDependentPacker.hpp"

namespace BidirectionalInMemGraph
{


struct HashIdConstructror
{
    static constexpr uint64_t HASH_64BIT_GRATIO_1 = 0x9E3779B97F4A7C15ull;
    static constexpr uint64_t HASH_64BIT_GRATIO_2 = 0xD6E8FEB86659FD93ull;


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
        FabricSegments EdgeTable{};
        HeaderIdentifierOfAPC PreviousSibling{HeaderIdentifierOfAPC::EOF_APC_HEADER};
        HeaderIdentifierOfAPC NextSibling{HeaderIdentifierOfAPC::EOF_APC_HEADER};
        HeaderIdentifierOfAPC InheritedEgdeTableIdx{HeaderIdentifierOfAPC::EOF_APC_HEADER};
        HeaderIdentifierOfAPC OwnedEgdeTableIdx{HeaderIdentifierOfAPC::EOF_APC_HEADER};
        HeaderIdentifierOfAPC RootOwnedChild{HeaderIdentifierOfAPC::EOF_APC_HEADER};
    };
    static_assert(sizeof(AxisConstructionMap) <= sizeof(uint64_t));

    static constexpr AxisConstructionMap ConstructAxisMap(BidirectionalAxis desired_axis) noexcept
    {
        if (desired_axis == BidirectionalAxis::HORIZONTALLY_SHARED)
        {
            return AxisConstructionMap{
                FabricSegments::HORIZONTAL_EDGE_TABLE,
                HeaderIdentifierOfAPC::PREVIOUS_HORIZONTAL_SLOT,
                HeaderIdentifierOfAPC::NEXT_HORIZONTAL_SLOT,
                HeaderIdentifierOfAPC::HORIZONTAL_SHARED_IDX,
                HeaderIdentifierOfAPC::HORIZONTAL_ROOT_IDX,
                HeaderIdentifierOfAPC::HORIZONTAL_NEXT_OF_ROOT
            };
        }

        return AxisConstructionMap{
            FabricSegments::VERTICAL_EDGE_TABLE,
            HeaderIdentifierOfAPC::PREVIOUS_VERTICAL_SLOT,
            HeaderIdentifierOfAPC::NEXT_VERTICAL_SLOT,
            HeaderIdentifierOfAPC::VERTICAL_SHARED_IDX,
            HeaderIdentifierOfAPC::VERTICAL_ROOT_IDX,
            HeaderIdentifierOfAPC::VERTICAL_NEXT_OF_ROOT

        };
    }

    static constexpr bool IsValidEven64(uint64_t value) noexcept
    {
        return 
            (value & 1u) == UNSIGNED_ZERO;
    }
};

struct DefineIdentityBuffer : public AxisConstructor
{
    static constexpr uint8_t IDENTITY_BUFFER_LEN = APCDataStructure::TotalIdentityUnitCount() + 1;
    static constexpr uint8_t IDENTITY_VALIDATION_IDX = IDENTITY_BUFFER_LEN - 1;
    static constexpr uint64_t VALIDATION_IDENTITY_MARK = 987987;



    enum class GraphMutationState : uint8_t
    {
        WRITE_LOCK = 0,
        CONSUME_LOCK = 1,
        LIVE = 2,
        HORIZONTAL_LOCK = 3,
        VERTICAL_LOCK = 4,
        INVALID = 5
    };

    using BufferOfAPCIdentity = std::array<uint64_t, IDENTITY_BUFFER_LEN>;

    static constexpr bool IsKnownIdentity(HeaderIdentifierOfAPC identity_unit) noexcept
    {
        return identity_unit >= HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT &&
            identity_unit <= HeaderIdentifierOfAPC::PREVIOUS_VERTICAL_SLOT;
    }

    static constexpr bool IsHoldFingerprintState(GraphMutationState state) noexcept
    {
        return state == GraphMutationState::WRITE_LOCK ||
            state == GraphMutationState::CONSUME_LOCK;
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
                identity == HeaderIdentifierOfAPC::VERTICAL_SHARED_IDX ||
                identity == HeaderIdentifierOfAPC::HORIZONTAL_SHARED_IDX ||
                identity == HeaderIdentifierOfAPC::HORIZONTAL_ROOT_IDX ||
                identity == HeaderIdentifierOfAPC::VERTICAL_ROOT_IDX ||
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

            const GraphMutationState state_fp = IdentityFingerprintToState(hash);


            if (
                state_fp != GraphMutationState::LIVE
            )
            {
                return 2u;
            }
            return hash;
        }


};


}