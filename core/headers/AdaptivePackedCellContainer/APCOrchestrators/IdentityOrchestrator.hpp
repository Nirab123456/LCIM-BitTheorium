#pragma once 
#include <array>
#include <utility>
#include "../CoreOFAPC/IdAndIdentityOfAPC.hpp"
#include "../../SharedComponents/BitPackers/ConAndCaDependentPacker.hpp"


namespace PredictedAdaptedEncoding
{

    struct IdentityBufferConf : public AxisConstructor
    {
        static constexpr uint8_t IDENTITY_BUFFER_LEN = APCDataStructure::TotalIdentityUnitCount() + 1;
        static constexpr uint8_t IDENTITY_VALIDATION_IDX = IDENTITY_BUFFER_LEN - 1;
        static constexpr uint64_t VALIDATION_IDENTITY_MARK = 987987;

        using BufferOfAPCIdentity = std::array<uint64_t, IDENTITY_BUFFER_LEN>;


        static constexpr bool IsKnownIdentity(HeaderIdentifierOfAPC identity_unit) noexcept
        {
            return identity_unit >= HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT &&
                identity_unit <= HeaderIdentifierOfAPC::ACCESS_PASSWORD;
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
            for (size_t i = 0; i < identity_buffer.size(); i++)
            {
                identity_buffer[i] = FABRIC_CELL_SENTINAL;
            }
        }

        static constexpr bool InsertAnIdentityInBuffer(
            BufferOfAPCIdentity& identity_buffer,
            HeaderIdentifierOfAPC identity,
            uint64_t value
        ) noexcept
        {
            const std::optional<uint8_t> buffer_idx = GetBufferIdxFromIdentityUnit(identity);
            if (
                !buffer_idx.has_value() ||
                !APCDataStructure::IsValidFabricUnit(value)
            )
            {
                return false;
            }

            identity_buffer[buffer_idx.value()] = value;
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

        static constexpr uint64_t ComposeIdentityFingerprint(
            const BufferOfAPCIdentity& identity_buffer
        ) noexcept
        {
            uint64_t hash = HASH_64BIT_GRATIO_1;
            for (uint8_t i = 1u; i < APCDataStructure::TotalIdentityUnitCount(); ++i)
            {
                hash ^= identity_buffer[i];
                hash *= HASH_64BIT_GRATIO_2;
                hash ^= identity_buffer[i];
                hash *= HASH_64BIT_GRATIO_3;
            }
            return hash == FABRIC_CELL_SENTINAL ? hash - 1u : hash;
        }


    };

    struct InstallAxisToBuffer : public IdentityBufferConf
    {
        static constexpr bool PlaceholderAxisCreation(
            BufferOfAPCIdentity& identity_buffer,
            BidirectionalAxis axis
        ) noexcept
        {
            const uint64_t branch_id = ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::BRANCH_ID);
            const AxisConstructionMap axis_construction_map = ConstructAxisMap(axis);
            const uint64_t placeholder_key = HashUnsigned64(branch_id);
            const std::optional<uint32_t> group_shared_id_and_prfefix = GroupPreFix32FromKey(placeholder_key);

            if (
                !IsValidAPCId(branch_id) ||
                !group_shared_id_and_prfefix.has_value()
            )
            {
                return false;
            }

            InsertAnIdentityInBuffer(identity_buffer, axis_construction_map.ID, group_shared_id_and_prfefix.value());
            InsertAnIdentityInBuffer(identity_buffer, axis_construction_map.CountTarget, UNSIGNED_ZERO);
            InsertAnIdentityInBuffer(identity_buffer, axis_construction_map.PreviousTarget, UNSIGNED_ZERO);
            InsertAnIdentityInBuffer(identity_buffer, axis_construction_map.NextTarget, UNSIGNED_ZERO);
            InsertAnIdentityInBuffer(identity_buffer, axis_construction_map.KEY, placeholder_key);
        }

        static constexpr bool IsValidAxisData(
            const BufferOfAPCIdentity& parent_buffer,
            BidirectionalAxis axis
        ) noexcept
        {
            bool is_parent = ValueOfAnIdentityFromBuffer(
                parent_buffer, 
                IsHorizontalSharedAxis(axis) ? HeaderIdentifierOfAPC::HORIZONTALLY_SHARED_COUNT :
                    HeaderIdentifierOfAPC::VARTICALLY_LOGICAL_COUNT
            ) > UNSIGNED_ZERO ? true : false;

            if (!is_parent)
            {
                return false;
            }

            const AxisConstructionMap axis_construction_map = ConstructAxisMap(axis);

            return IsValidAPCId(ValueOfAnIdentityFromBuffer(parent_buffer, axis_construction_map.ID)) &&
                IsValidAPCId(ValueOfAnIdentityFromBuffer(parent_buffer, axis_construction_map.CountTarget)) &&
                IsValidAPCId(ValueOfAnIdentityFromBuffer(parent_buffer, axis_construction_map.KEY)) &&
                ValueOfAnIdentityFromBuffer(parent_buffer, axis_construction_map.CountTarget) >=
                ValueOfAnIdentityFromBuffer(parent_buffer, axis_construction_map.NextTarget) &&
                ValueOfAnIdentityFromBuffer(parent_buffer, axis_construction_map.NextTarget) >=
                ValueOfAnIdentityFromBuffer(parent_buffer, axis_construction_map.PreviousTarget);
        }

        static constexpr bool ValidateAIdentityBuffer(
            BufferOfAPCIdentity& identity_buffer
        ) noexcept
        {
            if (
                !APCDataStructure::IsValidFabricUnit(ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::APC_SLOT_IDX)) ||
                !IsValidAPCId(ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::BRANCH_ID)) ||
                !IsValidAPCId(ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::ACCESS_PASSWORD))
            )
            {
                identity_buffer[IDENTITY_VALIDATION_IDX] = UNSIGNED_ZERO;
                return false;
            }

            if (
                !IsValidAxisData(identity_buffer, BidirectionalAxis::HORIZONTALLY_SHARED) ||
                !IsValidAxisData(identity_buffer, BidirectionalAxis::VARTICAL_LOGICAL)
            )
            {
                identity_buffer[IDENTITY_VALIDATION_IDX] = UNSIGNED_ZERO;
                return false;
            }

            const uint64_t fingerprint = ComposeIdentityFingerprint(identity_buffer);

            return fingerprint == ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT);
        }

        static constexpr bool MutateAxisAsChild(
            const BufferOfAPCIdentity& parent_identity_buffer,
            BufferOfAPCIdentity& own_identity_buffer,
            BidirectionalAxis axis
        ) noexcept;


    };
    
    

}