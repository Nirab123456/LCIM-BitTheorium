#pragma once 
#include <array>
#include <utility>
#include "AxisAndHash.hpp"


namespace PredictedAdaptedEncoding
{

    struct IdentityValidator : public DefineIdentityBuffer
    {
        enum class DescOfInharitance : uint8_t
        {
            FIRST_CHILD = 0,
            LINKED_CHILD = 1
        };

        static constexpr bool IsInheritedAxisDisabled(
            const BufferOfAPCIdentity& identity_buffer,
            BidirectionalAxis axis
        ) noexcept
        {
            const AxisConstructionMap map = ConstructAxisMap(axis);

            return
                ValueOfAnIdentityFromBuffer(identity_buffer, map.OrdinalKey) == FABRIC_CELL_SENTINAL &&
                ValueOfAnIdentityFromBuffer(identity_buffer, map.PreviousSibling) == FABRIC_CELL_SENTINAL &&
                ValueOfAnIdentityFromBuffer(identity_buffer, map.NextSibling) == FABRIC_CELL_SENTINAL;
        }

        static constexpr bool IsOwnedAxisDisabled(
            const BufferOfAPCIdentity& identity_buffer,
            BidirectionalAxis axis
        ) noexcept
        {
            const AxisConstructionMap map = ConstructAxisMap(axis);
            return 
                ValueOfAnIdentityFromBuffer(identity_buffer, map.OwnRootKey) == FABRIC_CELL_SENTINAL &&
                ValueOfAnIdentityFromBuffer(identity_buffer, map.RootOwnedChild) == FABRIC_CELL_SENTINAL;
        }

        static constexpr bool IsInharitedChild(
            const BufferOfAPCIdentity& identity_buffer,
            BidirectionalAxis axis
        ) noexcept
        {
            const AxisConstructionMap map = ConstructAxisMap(axis);
            const uint64_t key = ValueOfAnIdentityFromBuffer(identity_buffer, map.OrdinalKey);
            const std::optional<uint32_t> group_id = GroupPreFix32FromKey(key);
            const std::optional<uint32_t> group_ordinal = GetOrdinalFromKey(key);
            const uint64_t previous_slot = ValueOfAnIdentityFromBuffer(identity_buffer, map.PreviousSibling);
            const uint64_t next_slot = ValueOfAnIdentityFromBuffer(identity_buffer, map.NextSibling);

            return 
                group_id.has_value() &&
                group_ordinal.has_value() &&
                group_ordinal.value() > UNSIGNED_ZERO &&
                IsValidAPCSlotIdx(previous_slot) &&
                (IsValidAPCSlotIdx(next_slot) || next_slot == FABRIC_CELL_SENTINAL);
        }


        static constexpr bool IsValidInheritedAxis(
            const BufferOfAPCIdentity& identity_buffer,
            BidirectionalAxis axis
        ) noexcept
        {
            return 
                IsInheritedAxisDisabled(identity_buffer, axis) ||
                IsInharitedChild(identity_buffer, axis);
        }

        static constexpr bool IsDefinedRoot(
            const BufferOfAPCIdentity& identity_buffer,
            BidirectionalAxis axis
        ) noexcept
        {
            const AxisConstructionMap map = ConstructAxisMap(axis);
            const uint64_t root_key = ValueOfAnIdentityFromBuffer(identity_buffer, map.OwnRootKey);
            const uint64_t first_child = ValueOfAnIdentityFromBuffer(identity_buffer, map.RootOwnedChild);
            const std::optional<uint32_t> group_id = GroupPreFix32FromKey(root_key);
            const std::optional<uint32_t> ordinal = GetOrdinalFromKey(root_key);
            return 
                group_id.has_value() &&
                ordinal.has_value() &&
                ordinal.value() == UNSIGNED_ZERO &&
                (
                    first_child == FABRIC_CELL_SENTINAL ||
                    IsValidAPCSlotIdx(first_child)
                );
        }

        static constexpr bool IsValidOwnedRoot(
            const BufferOfAPCIdentity& identity_buffer,
            BidirectionalAxis axis
        ) noexcept
        {
            return 
                IsOwnedAxisDisabled(identity_buffer, axis) ||
                IsDefinedRoot(identity_buffer, axis);
        }


        static constexpr bool ValidateIdentityStructure(
            const BufferOfAPCIdentity& identity_buffer
        ) noexcept
        {
            const uint64_t slot = ValueOfAnIdentityFromBuffer(identity_buffer,  HeaderIdentifierOfAPC::APC_SLOT_IDX);
            const uint64_t begin = ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::BOUNDS_BEGIN);
            const uint64_t end = ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::BOUNDS_END);

            return 
                IsValidAPCSlotIdx(slot) &&
                APCDataStructure::IsValidFabricUnit(begin) &&
                APCDataStructure::IsValidFabricUnit(end) &&
                APCDataStructure::IsCapacityOfAPCValid(static_cast<uint32_t>(end - begin)) &&
                IsValidInheritedAxis(identity_buffer, BidirectionalAxis::HORIZONTALLY_SHARED) &&
                IsValidInheritedAxis(identity_buffer, BidirectionalAxis::VARTICAL_LOGICAL) &&
                IsValidOwnedRoot(identity_buffer, BidirectionalAxis::HORIZONTALLY_SHARED) &&
                IsValidOwnedRoot(identity_buffer, BidirectionalAxis::VARTICAL_LOGICAL);
        }


    };


    struct InstallAxisToBuffer : public IdentityValidator
    {

        static constexpr bool DisableInharitadAxis(
            BufferOfAPCIdentity& identity_buffer,
            BidirectionalAxis axis 
        ) noexcept
        {
            const AxisConstructionMap map = ConstructAxisMap(axis);

            return
                InsertAnIdentityInBuffer(identity_buffer, map.OrdinalKey, FABRIC_CELL_SENTINAL) &&
                InsertAnIdentityInBuffer(identity_buffer, map.PreviousSibling, FABRIC_CELL_SENTINAL) &&
                InsertAnIdentityInBuffer(identity_buffer, map.NextSibling, FABRIC_CELL_SENTINAL);
        }


        static constexpr bool DisableOwnedRoot(
            BufferOfAPCIdentity& identity_buffer,
            BidirectionalAxis axis
        ) noexcept
        {
            const AxisConstructionMap map = ConstructAxisMap(axis);

            return
                InsertAnIdentityInBuffer(identity_buffer, map.OwnRootKey, FABRIC_CELL_SENTINAL) &&
                InsertAnIdentityInBuffer(identity_buffer, map.RootOwnedChild, FABRIC_CELL_SENTINAL);
        }


        static constexpr bool InstallOwnedRoot(
            BufferOfAPCIdentity& identity_buffer,
            BidirectionalAxis axis,
            uint32_t axis_id,
            bool is_destructive = false
        ) noexcept
        {
            if (
                !ValidateIdentityStructure(identity_buffer) ||
                !IsValidGroupId(axis_id) ||
                (!is_destructive && !IsOwnedAxisDisabled(identity_buffer, axis))
            )
            {
                return false;
            }

            const AxisConstructionMap map = ConstructAxisMap(axis);

            const uint64_t root_key = ComposeNewGroupKey(
                axis_id,
                axis,
                UNSIGNED_ZERO
            );

            return 
                InsertAnIdentityInBuffer(identity_buffer, map.OwnRootKey, root_key) &&
                InsertAnIdentityInBuffer(identity_buffer, map.RootOwnedChild, FABRIC_CELL_SENTINAL);
        }


        static constexpr bool SealIdentityBuffer(
            BufferOfAPCIdentity& identity_buffer
        ) noexcept
        {
            const uint64_t fingerprint = ComposeIdentityFingerprint(identity_buffer);
            if (
                !ValidateIdentityStructure(identity_buffer) ||
                !InsertAnIdentityInBuffer(identity_buffer, HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT, fingerprint)
            )
            {
                identity_buffer[IDENTITY_VALIDATION_IDX] = UNSIGNED_ZERO;
                return false;
            }
            identity_buffer[IDENTITY_VALIDATION_IDX] = VALIDATION_IDENTITY_MARK;
            return true;
        }


        static constexpr bool ValidateAIdentityBuffer(
            BufferOfAPCIdentity& identity_buffer
        ) noexcept
        {
            const uint64_t fingerprint = ComposeIdentityFingerprint(identity_buffer);
            if (
                !ValidateIdentityStructure(identity_buffer) ||
                fingerprint != ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT) ||
                StateOfIdentityFingerprint(fingerprint) != FingerprintHashState::VALID
            )
            {
                identity_buffer[IDENTITY_VALIDATION_IDX] = UNSIGNED_ZERO;
                return false;
            }
            identity_buffer[IDENTITY_VALIDATION_IDX] = VALIDATION_IDENTITY_MARK;
            return true;
        }

        static constexpr FingerprintHashState GetStateFingerprint(
            BufferOfAPCIdentity& identity_buffer,
            uint64_t* fingerprint = nullptr
        ) noexcept
        {
            const uint64_t identity_value = ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT);
            if (!ValidateIdentityStructure(identity_buffer))
            {
                return FingerprintHashState::INVALID;
            }
            if (fingerprint)
            {
                *fingerprint = identity_value;
            }
            return StateOfIdentityFingerprint(identity_value);
        }


    };
    
    

}