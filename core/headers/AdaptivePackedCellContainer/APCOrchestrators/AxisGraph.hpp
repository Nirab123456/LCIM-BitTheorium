
#pragma once 
#include <array>
#include <utility>
#include "APCDataStructure.hpp"
#include "../../SharedComponents/BitPackers/ConAndCaDependentPacker.hpp"

namespace BidirectionalInMemGraph
{
struct AxisConstructor
{
    enum class BidirectionalAxis : uint8_t
    {
        HORIZONTAL = 1,
        VERTICAL = 2
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
        if (desired_axis == BidirectionalAxis::HORIZONTAL)
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

struct GraphLockConf : public AxisConstructor
{

    static constexpr bool IsValidAPCId(uint64_t value) noexcept
    {
        return value > UNSIGNED_ZERO && value < FABRIC_CELL_SENTINAL;
    }

    static constexpr bool IsValidGroupId(uint64_t value) noexcept
    {
        return value > UNSIGNED_ZERO && 
            APCDataStructure::IsValid32BitAPCUnit(value);
    }

    struct GraphMutationValues 
    {
        uint32_t SeqLock = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        uint32_t Flags = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        bool IsValid = false;
    };

    enum class MemGraphFlag : uint32_t
    {
        LIVE = 0u,
        HORIZONTAL_LOCK = 1u << 0u,
        VERTICAL_LOCK = 1u << 1u,
        BOTH_AXIS_LOCK = (1u << 0u) | (1u << 1u),
        READ_ONLY = 1u << 2u
    };

    static constexpr uint32_t GRAPH_FLAGS_MASK = 7u;

    static constexpr bool HasThisGraphMutationFlag(
        uint32_t raw_lock,
        MemGraphFlag desired_flag
    ) noexcept
    {
        return
            IsGraphFlagRawValid(FlagMask(desired_flag)) &&
            (raw_lock & FlagMask(desired_flag)) == FlagMask(desired_flag);
    }

    static constexpr uint32_t SetGraphFlags(uint32_t raw_lock, MemGraphFlag flag) noexcept
    {
        return raw_lock | FlagMask(flag);
    }

    static constexpr bool IsGraphFlagRawValid(uint32_t raw_flag) noexcept
    {
        bool hash_read_only = HasThisGraphMutationFlag(raw_flag, MemGraphFlag::READ_ONLY);
        bool hash_horizontal = HasThisGraphMutationFlag(raw_flag, MemGraphFlag::HORIZONTAL_LOCK);
        bool hash_vertical = HasThisGraphMutationFlag(raw_flag, MemGraphFlag::VERTICAL_LOCK);

        if (
            hash_read_only &&
            (
                hash_horizontal || hash_vertical
            )
        )
        {
            return false;
        }
        
        return 
            raw_flag <= GRAPH_FLAGS_MASK;
    }

    static constexpr bool IsIdentityGraphUnlocked(uint32_t raw) noexcept
    {
        return raw == FlagMask(MemGraphFlag::LIVE);
    }

    static constexpr bool GraphAxisMutable(uint32_t raw, BidirectionalAxis axis) noexcept
    {
        if (!IsGraphFlagRawValid(raw))
        {
            return false;
        }

        const MemGraphFlag rejected = (axis == BidirectionalAxis::HORIZONTAL) ?
            MemGraphFlag::HORIZONTAL_LOCK : MemGraphFlag::VERTICAL_LOCK;

        bool hash_read_only = HasThisGraphMutationFlag(raw, MemGraphFlag::READ_ONLY);
        bool hash_rejected = HasThisGraphMutationFlag(raw, rejected);

        return 
            !hash_read_only &&
            !hash_rejected;
    }

    static constexpr bool IsValidGraphMutationState(GraphMutationValues& mutations) noexcept
    {
        if (
            !IsGraphFlagRawValid(mutations.Flags)
        )
        {
            mutations.IsValid = false;
            return false;
        }

        bool unlocked = IsIdentityGraphUnlocked(mutations.Flags);
        bool hash_read_only = HasThisGraphMutationFlag(mutations.Flags, MemGraphFlag::READ_ONLY);

        if (
            unlocked ||
            hash_read_only
        )
        {
            mutations.IsValid = IsValidEven64(mutations.SeqLock);
        }
        else
        {
            mutations.IsValid = !IsValidEven64(mutations.SeqLock);
        }

        return mutations.IsValid;
    }

    static constexpr bool DoseCurrentFlagsAllowsThisAxisMutation(uint32_t flags, BidirectionalAxis axis) noexcept
    {
        switch (axis)
        {
        case BidirectionalAxis::HORIZONTAL:
            return 
                HasThisGraphMutationFlag(flags, MemGraphFlag::BOTH_AXIS_LOCK) ||
                HasThisGraphMutationFlag(flags, MemGraphFlag::HORIZONTAL_LOCK);
        case BidirectionalAxis::VERTICAL:
            return 
                HasThisGraphMutationFlag(flags, MemGraphFlag::BOTH_AXIS_LOCK) ||
                HasThisGraphMutationFlag(flags, MemGraphFlag::VERTICAL_LOCK);
        default:
            return false;
        }
    }

    static constexpr bool ExtractGraphMutationValues(
        uint64_t value,
        GraphMutationValues& values
    ) noexcept
    {
        values.SeqLock = TwinU32ToU64::ExtractLow32Of64(value);
        values.Flags = TwinU32ToU64::ExtractHigh32Of64(value);
        return IsValidGraphMutationState(values);
    }

    static constexpr uint64_t MakeGraphMutationRaw(const GraphMutationValues& values) noexcept
    {
        return TwinU32ToU64::PackDoubleUnsigned32In64(values.SeqLock, values.Flags);
    }


protected:
    static constexpr uint32_t FlagMask(MemGraphFlag flag) noexcept
    {
        return static_cast<uint32_t>(flag);
    }
};


struct DefineIdentityBuffer : public GraphLockConf
{
    using BufferOfAPCIdentity = std::array<uint64_t, APCDataStructure::TotalIdentityUnitCount()>;

    static constexpr bool IsKnownIdentity(HeaderIdentifierOfAPC identity_unit) noexcept
    {
        return identity_unit >= HeaderIdentifierOfAPC::GRAPH_MUTATION_AND_LOCK &&
            identity_unit <= HeaderIdentifierOfAPC::PREVIOUS_VERTICAL_SLOT;
    }

    static constexpr std::optional<uint8_t> GetBufferIdxFromIdentityUnit(HeaderIdentifierOfAPC identity_unit) noexcept
    {
        if (!IsKnownIdentity(identity_unit))
        {
            return std::nullopt;
        }
        
        return static_cast<uint8_t>(
            static_cast<uint8_t>(identity_unit) - static_cast<uint8_t>(HeaderIdentifierOfAPC::GRAPH_MUTATION_AND_LOCK)
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
            buffer_idx + static_cast<uint8_t>(HeaderIdentifierOfAPC::GRAPH_MUTATION_AND_LOCK)
        );
    }


        static constexpr void BuildNullIdentityBuffer(BufferOfAPCIdentity& identity_buffer) noexcept
        {
            identity_buffer.fill(FABRIC_CELL_SENTINAL);
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
            return true;
        }


        static constexpr bool InsertGraphIdentityMutation(
            BufferOfAPCIdentity& identity_buffer,
            GraphMutationValues& values
        ) noexcept
        {
            if (
                !IsValidGraphMutationState(values)
            )
            {
                return false;
            }
            const uint64_t mutation_lock = MakeGraphMutationRaw(values);
            return InsertAnIdentityInBuffer(identity_buffer, HeaderIdentifierOfAPC::GRAPH_MUTATION_AND_LOCK, mutation_lock);
        }

        static constexpr bool GetGraphMutationValues(
            const BufferOfAPCIdentity& identity_buffer,
            GraphMutationValues& values
        ) noexcept
        {
            const uint64_t mutation_lock_st = ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::GRAPH_MUTATION_AND_LOCK);
            return ExtractGraphMutationValues(mutation_lock_st, values);
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
                identity == HeaderIdentifierOfAPC::GRAPH_MUTATION_AND_LOCK ||
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

};


}