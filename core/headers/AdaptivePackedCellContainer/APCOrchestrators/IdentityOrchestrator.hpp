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
                identity_unit <= HeaderIdentifierOfAPC::ACCESS_PASSWORD;
        }

        static constexpr bool IsHoldFingerprintState(FingerprintHashState state) noexcept
        {
            return state == FingerprintHashState::WRITE_LOCK ||
                state == FingerprintHashState::CONSUME_LOCK;
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
            if (
                !buffer_idx.has_value() ||
                !(value == FABRIC_CELL_SENTINAL && CanIdentityContainRuntimeSentinal(identity))
            )
            {
                return false;
            }

            identity_buffer[buffer_idx.value()] = value;
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

        static constexpr bool CanIdentityContainRuntimeSentinal(HeaderIdentifierOfAPC identity) noexcept
        {

            return identity == HeaderIdentifierOfAPC::LOGICAL_GROUP_ID ||
                identity == HeaderIdentifierOfAPC::SHARED_GROUP_ID ||
                identity == HeaderIdentifierOfAPC::LOGICAL_ID_HASH_KEY ||
                identity == HeaderIdentifierOfAPC::SHARED_ID_HASH_KEY ||
                identity == HeaderIdentifierOfAPC::PREVIOUS_HORIZONTAL_HANDLE ||
                identity == HeaderIdentifierOfAPC::PREVIOUS_VERTICAL_HANDLE ||
                identity == HeaderIdentifierOfAPC::NEXT_HORIZONTAL_HANDLE ||
                identity == HeaderIdentifierOfAPC::NEXT_VERTICAL_HANDLE;
        }


    };


    struct InstallAxisToBuffer : public IdentityBufferConf
    {

        static constexpr bool IsAxisDisabled(
            const BufferOfAPCIdentity& identity_buffer,
            BidirectionalAxis axis
        ) noexcept
        {
            const AxisConstructionMap map = ConstructAxisMap(axis);

            return ValueOfAnIdentityFromBuffer(identity_buffer, map.ID) == FABRIC_CELL_SENTINAL &&
                ValueOfAnIdentityFromBuffer(identity_buffer, map.KEY) == FABRIC_CELL_SENTINAL &&
                ValueOfAnIdentityFromBuffer(identity_buffer, map.PreviousTarget) == FABRIC_CELL_SENTINAL &&
                ValueOfAnIdentityFromBuffer(identity_buffer, map.NextTarget) == FABRIC_CELL_SENTINAL &&
                ValueOfAnIdentityFromBuffer(identity_buffer, map.CountTarget) == UNSIGNED_ZERO;
        }

        static constexpr bool IsLinkedAxis(
            const BufferOfAPCIdentity& identity_buffer,
            BidirectionalAxis axis
        ) noexcept
        {
            if (IsAxisDisabled(identity_buffer, axis))
            {
                return true;
            }

            const AxisConstructionMap map = ConstructAxisMap(axis);

            const uint64_t group_id = ValueOfAnIdentityFromBuffer(identity_buffer, map.ID);
            const uint64_t key = ValueOfAnIdentityFromBuffer(identity_buffer, map.KEY);
            const uint32_t count_of_ordinal = static_cast<uint32_t>(ValueOfAnIdentityFromBuffer(identity_buffer, map.CountTarget));
            const uint64_t previous_handle = ValueOfAnIdentityFromBuffer(identity_buffer, map.PreviousTarget);
            const uint64_t next_handle = ValueOfAnIdentityFromBuffer(identity_buffer, map.NextTarget);

            if (
                !IsValidGroupId(group_id) ||
                !APCDataStructure::IsValid32BitAPCUnit(count_of_ordinal)
            )
            {
                return false;
            }

            const uint32_t group_prefix = static_cast<uint32_t>(group_id);
            const uint64_t parent_key = MakeGroupAccessKey(group_prefix , UNSIGNED_ZERO);
            const uint64_t top_ordinal_key = MakeGroupAccessKey(group_prefix, count_of_ordinal);

            if (!APCDataStructure::IsValidFabricUnit(previous_handle))
            {
                return key == parent_key &&
                    next_handle != UNSIGNED_ZERO;
            }
            
            return count_of_ordinal > UNSIGNED_ZERO &&
                key == top_ordinal_key &&
                IsValidAPCId(previous_handle) &&
                next_handle != UNSIGNED_ZERO;
            
        }

        static constexpr bool DoseBufferContainsIdentity(const BufferOfAPCIdentity& identity_buffer) noexcept
        {
            if (
                !IsValidAPCSlotIdx(ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::APC_SLOT_IDX)) ||
                !IsValidAPCId(ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::BRANCH_ID)) ||
                !IsValidAPCId(ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::ACCESS_PASSWORD)) ||
                !IsLinkedAxis(identity_buffer, BidirectionalAxis::HORIZONTALLY_SHARED) ||
                !IsLinkedAxis(identity_buffer, BidirectionalAxis::VARTICAL_LOGICAL)
            )
            {
                return false;
            }
            return true;
        }

        static constexpr bool IsValidEven64 (uint64_t value) noexcept
        {
            return APCDataStructure::IsValidFabricUnit(value) &&
                (value & 1u) == UNSIGNED_ZERO;
        }
        
        static constexpr bool ValidateAIdentityBuffer(
            BufferOfAPCIdentity& identity_buffer
        ) noexcept
        {
            if (
                !DoseBufferContainsIdentity(identity_buffer)
            )
            {
                identity_buffer[IDENTITY_VALIDATION_IDX] = UNSIGNED_ZERO;
                return false;
            }

            const uint64_t stored_fp = ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT);

            if (
                stored_fp == ComposeIdentityFingerprint(identity_buffer) &&
                IsValidEven64(stored_fp)
            )
            {
                identity_buffer[IDENTITY_VALIDATION_IDX] = VALIDATION_IDENTITY_MARK;
                return true;
            }

            identity_buffer[IDENTITY_VALIDATION_IDX] = UNSIGNED_ZERO;
            return false;
        }

        static constexpr bool SealIdentityBuffer(
            BufferOfAPCIdentity& identity_buffer
        ) noexcept
        {
            const uint64_t fingerprint = ComposeIdentityFingerprint(identity_buffer);
            if (
                !InsertAnIdentityInBuffer(identity_buffer, HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT, fingerprint)
            )
            {
                identity_buffer[IDENTITY_VALIDATION_IDX] = UNSIGNED_ZERO;
                return false;
            }
            return ValidateAIdentityBuffer(identity_buffer);
        }


        static constexpr FingerprintHashState GetStateFingerprint(
            BufferOfAPCIdentity& identity_buffer,
            uint64_t* fingerprint = nullptr
        ) noexcept
        {
            const uint64_t identity_value = identity_buffer[static_cast<uint8_t>(HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT)];
            if (fingerprint)
            {
                *fingerprint = identity_value;
            }
            if (!ValidateAIdentityBuffer(identity_buffer))
            {
                return FingerprintHashState::INVALID;
            }

            return StateOfIdentityFingerprint(identity_value);
        }



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

        static constexpr bool MutateAxisAsChild(
            const BufferOfAPCIdentity& parent_identity_buffer,
            BufferOfAPCIdentity& own_identity_buffer,
            BidirectionalAxis axis
        ) noexcept;


    };
    
    

}