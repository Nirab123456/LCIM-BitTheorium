#pragma once 
#include <array>
#include <utility>
#include "../CoreOFAPC/IdAndIdentityOfAPC.hpp"


namespace PredictedAdaptedEncoding
{

    struct IdentityValidator : public DefineIdentityBuffer
    {

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
                ValueOfAnIdentityFromBuffer(identity_buffer, map.PreviousInharitance) == FABRIC_CELL_SENTINAL &&
                ValueOfAnIdentityFromBuffer(identity_buffer, map.NextInharitance) == FABRIC_CELL_SENTINAL;
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
            const uint64_t previous_slot = ValueOfAnIdentityFromBuffer(identity_buffer, map.PreviousInharitance);
            const uint64_t next_slot = ValueOfAnIdentityFromBuffer(identity_buffer, map.NextInharitance);

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
                InsertAnIdentityInBuffer(identity_buffer, map.PreviousInharitance, FABRIC_CELL_SENTINAL) &&
                InsertAnIdentityInBuffer(identity_buffer, map.NextInharitance, FABRIC_CELL_SENTINAL);
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




        static constexpr bool InstallChildAxis(
            BufferOfAPCIdentity& parent_identity_buffer,
            BufferOfAPCIdentity& own_identity_buffer,
            BidirectionalAxis axis,
            bool is_destructive = false
        ) noexcept
        {
            if (
                (!IsValidInheritedAxis(parent_identity_buffer, axis)) ||
                (!is_destructive && !IsInheritedAxisDisabled(own_identity_buffer, axis))
            )
            {
                return false;
            }

            const AxisConstructionMap map = ConstructAxisMap(axis);
            const uint32_t apc_slot_idx = static_cast<uint32_t>(ValueOfAnIdentityFromBuffer(own_identity_buffer, HeaderIdentifierOfAPC::APC_SLOT_IDX));
            const uint64_t parent_key = ValueOfAnIdentityFromBuffer(parent_identity_buffer, map.OrdinalKey);
            const std::optional<uint32_t> parent_id = GroupPreFix32FromKey(parent_key);
            const std::optional<uint32_t> parent_ordinal = GetOrdinalFromKey(parent_key);
            const uint32_t next_of_parent = static_cast<uint32_t>(ValueOfAnIdentityFromBuffer(parent_identity_buffer, map.NextInharitance));

            if (
                !APCDataStructure::IsValid32BitAPCUnit(apc_slot_idx) ||
                !parent_id.has_value() ||
                !!parent_ordinal.has_value() ||
                !APCDataStructure::IsValid32BitAPCUnit(next_of_parent) ||
                next_of_parent != apc_slot_idx
            )
            {
                return false;
            }
            const uint64_t child_key = MakeGroupKeyFromParentGroupId(parent_id.value(), parent_ordinal.value() + 1);
            
            return
                InsertAnIdentityInBuffer(own_identity_buffer, map.OrdinalKey, child_key) &&
                InsertAnIdentityInBuffer(own_identity_buffer, map.PreviousInharitance, parent_ordinal.value()) &&
                InsertAnIdentityInBuffer(own_identity_buffer, map.NextInharitance, FABRIC_CELL_SENTINAL);
        }

        static constexpr bool MutateAxisAsChild(
            const BufferOfAPCIdentity& parent_identity_buffer,
            BufferOfAPCIdentity& own_identity_buffer,
            BidirectionalAxis axis
        ) noexcept;


    };
    
    

}