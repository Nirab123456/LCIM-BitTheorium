#pragma once 
#include "FabricTableOrchestrator.hpp"

namespace BidirectionalInMemGraph
{
struct EdgeTableConf : public DescriptionOfAPC
{

    using EdgeStatus = StateOfAPC;
    using DirtyRelationMask = uint64_t;
    using EdgeLockValues = SeqLockAndStateStruct;

    static constexpr uint32_t RELATION_NULL = APCDataStructure::APC_INDEX_BOUND_SENTINAL;

    static constexpr uint8_t COMPILED_MAX_DIRECTED_PARENTS_PER_AXIS = 64u;
    static constexpr uint8_t RELATION_ORDINAL = 8u;
    static constexpr uint8_t RELATION_SLOT_BITS = 24u;

    static constexpr uint8_t EDGE_SEQUENSE_BITS = LEN_OF_BYTE_IN_BITS * sizeof(uint32_t);

    static constexpr uint32_t RELATION_SLOT_MASK = MaskLowBitsForU32(RELATION_SLOT_BITS);
    static constexpr uint32_t EDGE_SEQUENSE_MASK = MaskLowBitsForU32(EDGE_SEQUENSE_BITS);
    static constexpr uint32_t EDGE_STATUS_MASK = 0x0fu;


    struct alignas(uint64_t) ParentRelation final
    {
        uint64_t ParentHandle = FABRIC_CELL_SENTINAL;
        uint64_t SiblingLocators = FABRIC_CELL_SENTINAL;
    };

    static_assert(sizeof(ParentRelation) == 2u * sizeof(uint64_t));
    static_assert(alignof(ParentRelation) == alignof(uint64_t));
    static_assert(std::is_trivially_copyable_v<ParentRelation>);

    struct EdgeData final
    {
        uint32_t SeqLock = UNSIGNED_ZERO;
        EdgeStatus Status = EdgeStatus::FREE;

        std::array<ParentRelation, COMPILED_MAX_DIRECTED_PARENTS_PER_AXIS> Parents{};
    };

    static constexpr size_t EdgeTableRecordWidth(uint8_t max_direct_parent) noexcept
    {
        return 
            1u + static_cast<size_t>(max_direct_parent) * 
            (sizeof(ParentRelation) / sizeof(uint64_t));
    }

    static constexpr bool IsValidConfigurableParentCapacity(uint8_t value) noexcept
    {
        return  value > UNSIGNED_ZERO && value <= COMPILED_MAX_DIRECTED_PARENTS_PER_AXIS;
    }

    static constexpr uint32_t PackRelationAllocator(uint32_t apc_slot, uint8_t relation_ordinal) noexcept
    {
        return 
            (static_cast<uint32_t>(relation_ordinal) << RELATION_SLOT_BITS) | apc_slot;
    }

    static constexpr uint32_t RelationSlot(uint32_t locator) noexcept
    {
        return locator & RELATION_SLOT_MASK;
    }

    static constexpr uint8_t RelationOrdinal(uint32_t locator) noexcept
    {
        return static_cast<uint8_t>(locator >> RELATION_SLOT_BITS);
    }

    static constexpr uint64_t FirstChildCellFromLocator(uint32_t locator) noexcept
    {
        return locator == RELATION_NULL ?
            FABRIC_CELL_SENTINAL : static_cast<uint64_t>(locator);
    }

    static constexpr uint32_t LocatorFromFirstChild(uint64_t raw) noexcept
    {
        return raw == FABRIC_CELL_SENTINAL ?
            RELATION_NULL : static_cast<uint32_t>(raw);
    }

    static constexpr uint64_t MakeParentHandle(uint32_t parent_slot, uint32_t parent_generation) noexcept
    {
        return TwinU32ToU64::PackDoubleUnsigned32In64(parent_slot, parent_generation);
    }

    static constexpr uint32_t ParentSlot(const ParentRelation& relation) noexcept
    {
        return TwinU32ToU64::ExtractLow32Of64(relation.ParentHandle);
    }

    static constexpr uint32_t ParentGeneration(const ParentRelation& relation) noexcept
    {
        return TwinU32ToU64::ExtractHigh32Of64(relation.ParentHandle);
    }

    static constexpr uint32_t PreviousLocator(const ParentRelation& relation) noexcept
    {
        return TwinU32ToU64::ExtractHigh32Of64(relation.SiblingLocators);
    }

    static constexpr uint32_t NextLocator(const ParentRelation& relation) noexcept
    {
        return TwinU32ToU64::ExtractHigh32Of64(relation.SiblingLocators);
    }

    static constexpr void setSibblingLocators(
        ParentRelation& relation,
        uint32_t previous,
        uint32_t next
    ) noexcept
    {
        relation.SiblingLocators = TwinU32ToU64::PackDoubleUnsigned32In64(previous, next);
    }

    static constexpr ParentRelation MakeParentRelation(
        uint32_t parent_slot,
        uint32_t parent_generation,
        uint32_t previous,
        uint32_t next
    ) noexcept
    {
        return ParentRelation{
            MakeParentHandle(parent_slot, parent_generation),
            TwinU32ToU64::PackDoubleUnsigned32In64(previous, next)
        };
    }

    static constexpr bool IsEmpty(const ParentRelation& relation) noexcept
    {
        return relation.ParentHandle == FABRIC_CELL_SENTINAL &&
            relation.SiblingLocators == FABRIC_CELL_SENTINAL;
    }

    static constexpr void Clear(ParentRelation& relation) noexcept
    {
        relation = ParentRelation{};
    }

    static constexpr uint32_t NextGeneration(uint32_t current) noexcept
    {
        return (current + 1u) & EDGE_SEQUENSE_MASK;
    }

    static constexpr uint64_t PackEdgeHeader(
        uint32_t sequense,
        EdgeStatus status
    ) noexcept
    {
        return 
            static_cast<uint64_t>(sequense) |
            (static_cast<uint64_t>(status) << EDGE_SEQUENSE_BITS);
    }

    static constexpr void UnpackEdgeHader(uint64_t raw, EdgeData& edge) noexcept
    {
        edge.SeqLock = static_cast<uint32_t>(raw);
        edge.Status = static_cast<EdgeStatus>((raw >> EDGE_SEQUENSE_BITS) & EDGE_STATUS_MASK);
    }

    static constexpr void UnpackEdgeLock(uint64_t raw, EdgeLockValues& edge) noexcept
    {
        edge.SeqLock = static_cast<uint32_t>(raw);
        edge.StateOfTheAPC = static_cast<EdgeStatus>((raw >> EDGE_SEQUENSE_BITS) & EDGE_STATUS_MASK);
    }

    static constexpr DirtyRelationMask DirtyBit(uint8_t ordinal) noexcept
    {
        return uint64_t{1u} << ordinal;
    }


};


struct EdgeBuilder : public EdgeTableConf
{


};

}