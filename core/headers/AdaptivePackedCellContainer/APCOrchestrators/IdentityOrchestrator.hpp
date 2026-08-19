#pragma once 
#include <array>
#include <utility>
#include "AxisGraph.hpp"


namespace BidirectionalInMemGraph
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
                ValueOfAnIdentityFromBuffer(identity_buffer, map.InheritedEgdeTableIdx) == FABRIC_CELL_SENTINAL &&
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
                ValueOfAnIdentityFromBuffer(identity_buffer, map.OwnedEgdeTableIdx) == FABRIC_CELL_SENTINAL &&
                ValueOfAnIdentityFromBuffer(identity_buffer, map.RootOwnedChild) == FABRIC_CELL_SENTINAL;
        }

        static constexpr bool IsInharitedChild(
            const BufferOfAPCIdentity& identity_buffer,
            BidirectionalAxis axis
        ) noexcept
        {
            const AxisConstructionMap map = ConstructAxisMap(axis);
            const uint64_t inharited_edge_idx = ValueOfAnIdentityFromBuffer(identity_buffer, map.InheritedEgdeTableIdx);
            const uint64_t previous_slot = ValueOfAnIdentityFromBuffer(identity_buffer, map.PreviousSibling);
            const uint64_t next_slot = ValueOfAnIdentityFromBuffer(identity_buffer, map.NextSibling);

            return 
                APCDataStructure::IsValid32BitAPCUnit(inharited_edge_idx) &&
                APCDataStructure::IsValid32BitAPCUnit(previous_slot) &&
                (APCDataStructure::IsValid32BitAPCUnit(next_slot) || next_slot == FABRIC_CELL_SENTINAL);
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
            const uint64_t own_edge_idx = ValueOfAnIdentityFromBuffer(identity_buffer, map.OwnedEgdeTableIdx);
            const uint64_t first_child = ValueOfAnIdentityFromBuffer(identity_buffer, map.RootOwnedChild);
            const uint64_t own_slot_idx = ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::APC_SLOT_IDX);
            return 
                APCDataStructure::IsValid32BitAPCUnit(own_edge_idx) &&
                (
                    first_child == FABRIC_CELL_SENTINAL ||
                    APCDataStructure::IsValid32BitAPCUnit(first_child)
                ) &&
                own_slot_idx == own_edge_idx;
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

        static constexpr bool ValidateDefaultIdentity(
            const BufferOfAPCIdentity& identity_buffer
        ) noexcept
        {
            const uint64_t slot = ValueOfAnIdentityFromBuffer(identity_buffer,  HeaderIdentifierOfAPC::APC_SLOT_IDX);
            const uint64_t begin = ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::BOUNDS_BEGIN);
            const uint64_t end = ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::BOUNDS_END);

            return 
                APCDataStructure::IsValid32BitAPCUnit(slot) &&
                APCDataStructure::IsValidFabricUnit(begin) &&
                begin < end &&
                APCDataStructure::IsValidFabricUnit(end) &&
                APCDataStructure::IsCapacityOfAPCValid(end - begin) &&
                IsValidInheritedAxis(identity_buffer, BidirectionalAxis::HORIZONTAL) &&
                IsValidInheritedAxis(identity_buffer, BidirectionalAxis::VERTICAL) &&
                IsValidOwnedRoot(identity_buffer, BidirectionalAxis::HORIZONTAL) &&
                IsValidOwnedRoot(identity_buffer, BidirectionalAxis::VERTICAL);
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
                InsertAnIdentityInBuffer(identity_buffer, map.InheritedEgdeTableIdx, FABRIC_CELL_SENTINAL) &&
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
                InsertAnIdentityInBuffer(identity_buffer, map.OwnedEgdeTableIdx, FABRIC_CELL_SENTINAL) &&
                InsertAnIdentityInBuffer(identity_buffer, map.RootOwnedChild, FABRIC_CELL_SENTINAL);
        }

        static constexpr bool SealIdentityBuffer(
            BufferOfAPCIdentity& identity_buffer
        ) noexcept
        {
            return 
                ValidateDefaultIdentity(identity_buffer);
        }

        static constexpr bool ValidateIdentityBuffer(
            const BufferOfAPCIdentity& identity_buffer
        ) noexcept
        {
            GraphMutationValues values{};
            return 
                ValidateDefaultIdentity(identity_buffer) &&
                GetGraphMutationValues(identity_buffer, values) &&
                IsValidGraphMutationState(values);
        }

        static constexpr bool IdentityBufferFromSegmentPoolRange(
            uint32_t apc_slot_idx,
            const RangeOfAPC& range_of_segmentpool,
            BufferOfAPCIdentity& buffer
        ) noexcept
        {

            BuildNullIdentityBuffer(buffer);
            if (
                !range_of_segmentpool.IsValid ||
                !InsertAnIdentityInBuffer(buffer, HeaderIdentifierOfAPC::APC_SLOT_IDX, apc_slot_idx) ||
                !InsertAnIdentityInBuffer(buffer, HeaderIdentifierOfAPC::BOUNDS_BEGIN, range_of_segmentpool.BeginIndex) ||
                !InsertAnIdentityInBuffer(buffer, HeaderIdentifierOfAPC::BOUNDS_END, range_of_segmentpool.EndIndex)
            )
            {
                return false;
            }
            

            GraphMutationValues values{};
            values.Flags = static_cast<uint32_t>(MemGraphFlag::LIVE);
            values.SeqLockVertical = UNSIGNED_ZERO;
            values.SeqLockHorizontal = UNSIGNED_ZERO;
            return 
                InsertGraphIdentityMutation(buffer, values) &&
                SealIdentityBuffer(buffer);
        }


    };
    
    

}