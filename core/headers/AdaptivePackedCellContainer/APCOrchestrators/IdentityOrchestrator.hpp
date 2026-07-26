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

        // static constexpr bool ContainsRuntimeIdentity(
        //     BufferOfAPCIdentity& identity_buffer
        // ) noexcept
        // {

        // }

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


        static constexpr bool IsValidInheritedAxis(
            const BufferOfAPCIdentity& identity_buffer,
            BidirectionalAxis axis
        ) noexcept
        {
            if (IsInheritedAxisDisabled(identity_buffer, axis))
            {
                return true;
            }

            const AxisConstructionMap map = ConstructAxisMap(axis);

            const uint64_t key = ValueOfAnIdentityFromBuffer(identity_buffer, map.OrdinalKey);
            const std::optional<uint32_t> group_id = GroupPreFix32FromKey(key);
            const std::optional<uint32_t> group_ordinal = GetOrdinalFromKey(key);
            const uint64_t previous_slot = ValueOfAnIdentityFromBuffer(identity_buffer, map.PreviousSibling);
            const uint64_t next_slot = ValueOfAnIdentityFromBuffer(identity_buffer, map.NextSibling);

            return 
                group_id.has_value() &&
                group_ordinal.has_value() &&
                APCDataStructure::IsValid32BitAPCUnit(previous_slot) &&
                (APCDataStructure::IsValid32BitAPCUnit(next_slot) || next_slot == FABRIC_CELL_SENTINAL);
        }


        static constexpr bool IsValidOwnedRoot(
            const BufferOfAPCIdentity& identity_buffer,
            BidirectionalAxis axis
        ) noexcept
        {
            const AxisConstructionMap map = ConstructAxisMap(axis);

            const uint64_t root_key = ValueOfAnIdentityFromBuffer(identity_buffer, map.OwnRootKey);
            const uint64_t first_child = ValueOfAnIdentityFromBuffer(identity_buffer, map.RootOwnedChild);

            return 
                IsValidGroupId(root_key) &&
                APCDataStructure::IsValid32BitAPCUnit(first_child);
        }


        static constexpr bool ValidateAIdentityBuffer(
            BufferOfAPCIdentity& identity_buffer
        ) noexcept
        {
            const uint64_t slot = ValueOfAnIdentityFromBuffer(identity_buffer,  HeaderIdentifierOfAPC::APC_SLOT_IDX);
            const uint64_t begin = ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::BOUNDS_BEGIN);
            const uint64_t end = ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::BOUNDS_END);

            return 
                IsValidAPCSlotIdx(slot) &&
                APCDataStructure::IsValidFabricUnit(begin) &&
                APCDataStructure::IsValidFabricUnit(end) &&
                APCDataStructure::IsCapacityOfAPCValid(static_cast<uint32_t>(end - begin + 1)) &&
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
            uint32_t first_child_slot_idx,
            bool is_destructive = false
        ) noexcept
        {
            if (
                !ValidateAIdentityBuffer(identity_buffer) ||
                (!is_destructive && IsValidOwnedRoot(identity_buffer, axis)) ||
                !IsValidAPCSlotIdx(first_child_slot_idx)
            )
            {
                return false;
            }

            const uint64_t slot_idx = ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::APC_SLOT_IDX);
            const AxisConstructionMap map = ConstructAxisMap(axis);

            const uint64_t root_key = ComposeNewGroupKey(
                APCSlotIdxToHashTableHandler(slot_idx),
                axis,
                UNSIGNED_ZERO
            );

            return 
                InsertAnIdentityInBuffer(identity_buffer, map.OwnRootKey, root_key) &&
                InsertAnIdentityInBuffer(identity_buffer, map.RootOwnedChild, first_child_slot_idx);
        }

        static constexpr bool PrepareInharitedAxis(
            BufferOfAPCIdentity& parent_identity,
            BufferOfAPCIdentity& identity_buffer,
            BidirectionalAxis axis,
            DescOfInharitance inharitance
        ) noexcept
        {
            if (
                !ValidateAIdentityBuffer(parent_identity) ||
                !ValidateAIdentityBuffer(identity_buffer)
            )
            {
                return false;
            }

            const AxisConstructionMap map = ConstructAxisMap(axis);
            const uint32_t parent_slot = static_cast<uint32_t>(ValueOfAnIdentityFromBuffer(parent_identity, HeaderIdentifierOfAPC::APC_SLOT_IDX));
            const uint32_t current_slot = static_cast<uint32_t>(ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::APC_SLOT_IDX));

            uint64_t parent_key = UNSIGNED_ZERO;
            uint32_t parent_id = UNSIGNED_ZERO;
            uint32_t parent_ordinal = UNSIGNED_ZERO;

            switch (inharitance)
            {
            case DescOfInharitance::FIRST_CHILD:
                if (
                    (!IsValidOwnedRoot(parent_identity, axis) &&
                    !InstallOwnedRoot(parent_identity, axis, current_slot, false)) ||
                    !InsertAnIdentityInBuffer(parent_identity, map.RootOwnedChild, current_slot)
                )
                {
                    return false;
                }
                parent_key = ValueOfAnIdentityFromBuffer(parent_identity, map.OwnRootKey);
                parent_id = static_cast<uint32_t>(parent_key);
                parent_ordinal = UNSIGNED_ZERO;
                break;

            case DescOfInharitance::LINKED_CHILD:
                if (
                    !IsValidInheritedAxis(parent_identity, axis) ||
                    ValueOfAnIdentityFromBuffer(parent_identity, map.NextSibling) != FABRIC_CELL_SENTINAL ||
                    !InsertAnIdentityInBuffer(parent_identity, map.NextSibling, current_slot)
                )
                {
                    return false;
                }
                parent_key = ValueOfAnIdentityFromBuffer(parent_identity, map.OrdinalKey);
                parent_id = static_cast<uint32_t>(parent_key);
                parent_ordinal = GetOrdinalFromKey(parent_key).value();
                break;

            default:
                return false;
            }

            const uint64_t own_key = MakeGroupKeyFromParentGroupId(parent_id, parent_ordinal + 1);

            if (
                !InsertAnIdentityInBuffer(identity_buffer, map.OrdinalKey, own_key) ||
                !InsertAnIdentityInBuffer(identity_buffer, map.PreviousSibling, parent_slot) ||
                !InsertAnIdentityInBuffer(identity_buffer, map.NextSibling, FABRIC_CELL_SENTINAL)
            )
            {
                return false;
            }
            
            return SealIdentityBuffer(parent_identity) &&
                SealIdentityBuffer(identity_buffer);
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
            const uint64_t identity_value = ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT);
            if (!ValidateAIdentityBuffer(identity_buffer))
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