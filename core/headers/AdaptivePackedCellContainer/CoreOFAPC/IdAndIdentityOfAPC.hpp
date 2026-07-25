
#pragma once 
#include <array>
#include <utility>
#include "ConstructorsAndCarriersOfAPC.hpp"

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

    /// @brief CREATS: HASH KeyAndID: Based On a Desired SHARED / LOGICAL Group ID
    /// @param child_ordinal ORDINAL IDX < UINT32_MAX - 1
    /// @return IF INVALID: UINT64_MAX
    static constexpr uint64_t MakeGroupKeyFromParentGroupId(uint64_t group_id, uint32_t child_ordinal) noexcept
    {
        if (
            !IsValidGroupId(group_id) ||
            !APCDataStructure::IsValid32BitAPCUnit(child_ordinal)
        )
        {
            return FABRIC_CELL_SENTINAL;
        }

        return ((group_id & GROUP_PREFIX_MASK) << GROUP_IDX_BIT_BOUNDRY) | static_cast<uint64_t>(child_ordinal);
    }

    /// @brief Get 32 Bit Prefix of A Group Key Can be used to set a Different sequential idx to find linked APC's
    /// @param group_key Raw key
    /// @return If Key VALID: -> 32 BIT PREFIX / std::nullopt
    static constexpr std::optional<uint32_t> GroupPreFix32FromKey(uint64_t group_key) noexcept
    {
        if (!IsValidAPCId(group_key))
        {
            return std::nullopt;
        }
        const uint32_t prefix_32 = static_cast<uint32_t>((group_key >> GROUP_IDX_BIT_BOUNDRY) & GROUP_PREFIX_MASK);
        return IsValidGroupId(prefix_32) ? std::optional<uint32_t>(prefix_32) : std::nullopt;
    }

    /// @brief Get 16 Bit Sequential Linked Idx From Key
    /// @param group_key Raw Key
    /// @return if Key VALID: -> 16 BIT SEQUENTIAL IDX / std::nullopt
    static constexpr std::optional<uint32_t> GetOrdinalFromKey(uint64_t group_key) noexcept
    {
        if (!IsValidAPCId(group_key))
        {
            return std::nullopt;
        }

        const uint32_t ordinal = static_cast<uint32_t>(group_key & GROUP_SEQUENTIAL_INDEX_MASK);
        return IsValidGroupId(ordinal) ? ordinal : std::optional<uint32_t>(std::nullopt);
    }

    static constexpr uint64_t RebuildOriginalKey(uint32_t prefix32, uint32_t index_32) noexcept
    {
        const uint64_t original_key = (static_cast<uint64_t>(prefix32) << GROUP_IDX_BIT_BOUNDRY | static_cast<uint64_t>(index_32));

        return IsValidAPCId(original_key) ? original_key : FABRIC_CELL_SENTINAL;
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
        HeaderIdentifierOfAPC PreviousTarget{HeaderIdentifierOfAPC::EOF_APC_HEADER};
        HeaderIdentifierOfAPC NextTarget{HeaderIdentifierOfAPC::EOF_APC_HEADER};
        HeaderIdentifierOfAPC KeyAndID{HeaderIdentifierOfAPC::EOF_APC_HEADER};
        HeaderIdentifierOfAPC OwnRootKey{HeaderIdentifierOfAPC::EOF_APC_HEADER};
    };
    static_assert(sizeof(AxisConstructionMap) <= sizeof(uint64_t));

    static constexpr AxisConstructionMap ConstructAxisMap(BidirectionalAxis desired_axis) noexcept
    {
        if (desired_axis == BidirectionalAxis::HORIZONTALLY_SHARED)
        {
            return AxisConstructionMap{
                FabricTableSegmentClasses::HORIZONTAL_HASH,
                HeaderIdentifierOfAPC::PREVIOUS_HORIZONTAL_SLOT,
                HeaderIdentifierOfAPC::NEXT_HORIZONTAL_HANDLE,
                HeaderIdentifierOfAPC::HORIZONTAL_ORDINAL_KEY,
                HeaderIdentifierOfAPC::HORIZONTAL_ROOT_KEY
            };
        }

        return AxisConstructionMap{
            FabricTableSegmentClasses::VERTICAL_HASH,
            HeaderIdentifierOfAPC::PREVIOUS_VERTICAL_SLOT,
            HeaderIdentifierOfAPC::NEXT_VERTICAL_HANDLE,
            HeaderIdentifierOfAPC::VERTICAL_ORDINAL_KEY,
            HeaderIdentifierOfAPC::VERTICAL_ROOT_KEY
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
        uint64_t branch_id,
        BidirectionalAxis axis
    ) noexcept
    {
        const uint64_t axis_salt = IsHorizontalSharedAxis(axis) ? HASH_64BIT_GRATIO_1 : HASH_64BIT_GRATIO_2;
        uint32_t group_id = static_cast<uint32_t>(
            HashUnsigned64(branch_id ^ axis_salt)
        );
        if (!IsValidGroupId(group_id))
        {
            group_id ^= HASH_64BIT_GRATIO_3;
            group_id = IsValidAPCId(group_id) ? group_id : 1u;
        }
        return group_id;
    }


    static constexpr uint64_t HashGroupId(
        uint64_t branch_id,
        BidirectionalAxis axis,
        uint32_t ordinal
    ) noexcept
    {
        return MakeGroupKeyFromParentGroupId(
            DeriveGroupId(branch_id, axis),
            ordinal
        );
    }

};


struct APCGroupReserver : public AxisConstructor
{
    enum class APCIdentityDef : uint8_t
    {
        UNASSIGNED_UNUSED_NANNULL = 0,
        ROOT = 1,
        CHILD = 2,
        DEFAULT_ASSIGNMENT = 3,
        NULL_USER_INSTRUCTION = 4
    };

    struct APCInitialIdentityStruct
    {
        uint64_t APCSlotIndex = FABRIC_CELL_SENTINAL;
        uint64_t BranchID = FABRIC_CELL_SENTINAL;
        uint64_t SharedGroupId = FABRIC_CELL_SENTINAL;
        uint64_t LogicalGroupId = FABRIC_CELL_SENTINAL;

        uint64_t AccessPassword = FABRIC_CELL_SENTINAL;
        uint64_t SharedHashKey = FABRIC_CELL_SENTINAL;
        uint64_t LogicalHashKey = FABRIC_CELL_SENTINAL;

        uint64_t SharedPreviousHandle = FABRIC_CELL_SENTINAL;
        uint64_t SharedNextHandle = FABRIC_CELL_SENTINAL;
        uint64_t LogicalPreviousHandle = FABRIC_CELL_SENTINAL;
        uint64_t LogicalNextHandle = FABRIC_CELL_SENTINAL;

        uint32_t SharedSequentialCount = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        uint32_t LogicalSequentalCount = APCDataStructure::APC_INDEX_BOUND_SENTINAL;

        bool IsAssignable = false;
        APCIdentityDef HorizontalSharedState = APCIdentityDef::UNASSIGNED_UNUSED_NANNULL;
        APCIdentityDef VarticalLogicState  = APCIdentityDef::UNASSIGNED_UNUSED_NANNULL;
        bool IsAPCRootOfThisLocic = false;
    };

    static constexpr bool IsRequestedAxisValid(APCIdentityDef desired_axis) noexcept
    {
        return desired_axis != APCIdentityDef::UNASSIGNED_UNUSED_NANNULL;
    }

    static constexpr bool IfSystemDefinedAxis(APCIdentityDef desired_axis) noexcept
    {
        return desired_axis == APCIdentityDef::ROOT ||
            desired_axis == APCIdentityDef::CHILD ||
            desired_axis == APCIdentityDef::NULL_USER_INSTRUCTION;
    }

    static constexpr bool IsMinimalValidCreateRequestOfAPC(const APCInitialIdentityStruct& requested_conf) noexcept
    {
        return IsRequestedAxisValid(requested_conf.HorizontalSharedState) &&
            IsRequestedAxisValid(requested_conf.VarticalLogicState);
    }

    static constexpr bool IfSystemResolvedIdentityValid(APCInitialIdentityStruct& requested_conf) noexcept
    {
        requested_conf.IsAssignable = (
            HashIdConstructror::IsValidAPCSlotIdx(requested_conf.APCSlotIndex) &&
            HashIdConstructror::IsValidAPCId(requested_conf.BranchID) &&
            HashIdConstructror::IsValidAPCId(
                HashIdConstructror::APCSlotIdxToHashTableHandler(requested_conf.APCSlotIndex)
            ) &&
            HashIdConstructror::IsValidAPCId(requested_conf.AccessPassword) &&
            IfSystemDefinedAxis(requested_conf.HorizontalSharedState) &&
            IfSystemDefinedAxis(requested_conf.VarticalLogicState)
        );
        return requested_conf.IsAssignable;
    }

    static constexpr APCInitialIdentityStruct MakeDefaultIdentityForAPC() noexcept
    {
        APCInitialIdentityStruct requested_identity{};
        requested_identity.SharedSequentialCount = UNSIGNED_ZERO;
        requested_identity.LogicalSequentalCount = UNSIGNED_ZERO;
        requested_identity.HorizontalSharedState = APCIdentityDef::NULL_USER_INSTRUCTION;
        requested_identity.VarticalLogicState = APCIdentityDef::NULL_USER_INSTRUCTION;
        return requested_identity;
    }

    static constexpr APCInitialIdentityStruct MakeAGroupedIdentityForAPC(
        uint64_t shared_id,
        uint64_t logical_id
    ) noexcept
    {

        APCInitialIdentityStruct user_defined_identity = MakeDefaultIdentityForAPC();

        if (HashIdConstructror::IsValidAPCId(shared_id))
        {
            user_defined_identity.SharedGroupId = shared_id;
            user_defined_identity.HorizontalSharedState = APCIdentityDef::DEFAULT_ASSIGNMENT;
        }

        if (HashIdConstructror::IsValidAPCId(logical_id))
        {
            user_defined_identity.LogicalGroupId = logical_id;
            user_defined_identity.VarticalLogicState = APCIdentityDef::DEFAULT_ASSIGNMENT;
        }
        return user_defined_identity;
    }

    static constexpr APCIdentityDef RuntimeAxisIdentityResolved(BidirectionalAxis desired_axis, APCInitialIdentityStruct a_runtime_identity) noexcept
    {
        if (
            !IsMinimalValidCreateRequestOfAPC(a_runtime_identity) ||
            !HashIdConstructror::IsValidAPCId(a_runtime_identity.BranchID)
        )
        {
            return APCIdentityDef::UNASSIGNED_UNUSED_NANNULL;
        }
        
        if (desired_axis == BidirectionalAxis::HORIZONTALLY_SHARED)
        {
            if (a_runtime_identity.HorizontalSharedState == APCIdentityDef::DEFAULT_ASSIGNMENT)
            {
                a_runtime_identity.SharedGroupId = HashIdConstructror::MakeGroupKeyFromParentGroupId(a_runtime_identity.BranchID, UNSIGNED_ZERO);
                return APCIdentityDef::DEFAULT_ASSIGNMENT;
            }

            switch (a_runtime_identity.HorizontalSharedState)
            {
            case APCIdentityDef::DEFAULT_ASSIGNMENT:
                a_runtime_identity.SharedGroupId = HashIdConstructror::MakeGroupKeyFromParentGroupId(a_runtime_identity.BranchID, UNSIGNED_ZERO);
                return APCIdentityDef::DEFAULT_ASSIGNMENT;

            case APCIdentityDef::NULL_USER_INSTRUCTION:
                return APCIdentityDef::NULL_USER_INSTRUCTION;
                        
            default:
                if (!HashIdConstructror::IsValidAPCId(a_runtime_identity.SharedGroupId))
                {
                    return APCIdentityDef::UNASSIGNED_UNUSED_NANNULL;
                }
                return a_runtime_identity.HorizontalSharedState;
            }
        }
        else
        {
            if (a_runtime_identity.VarticalLogicState == APCIdentityDef::DEFAULT_ASSIGNMENT)
            {
                a_runtime_identity.LogicalGroupId = HashIdConstructror::MakeGroupKeyFromParentGroupId(a_runtime_identity.BranchID, UNSIGNED_ZERO);
                return APCIdentityDef::DEFAULT_ASSIGNMENT;
            }

            switch (a_runtime_identity.VarticalLogicState)
            {
            case APCIdentityDef::DEFAULT_ASSIGNMENT:
                a_runtime_identity.LogicalGroupId = HashIdConstructror::MakeGroupKeyFromParentGroupId(a_runtime_identity.BranchID, UNSIGNED_ZERO);
                return APCIdentityDef::DEFAULT_ASSIGNMENT;

            case APCIdentityDef::NULL_USER_INSTRUCTION:
                return APCIdentityDef::NULL_USER_INSTRUCTION;
                        
            default:
                if (!HashIdConstructror::IsValidAPCId(a_runtime_identity.LogicalGroupId))
                {
                    return APCIdentityDef::UNASSIGNED_UNUSED_NANNULL;
                }
                return a_runtime_identity.VarticalLogicState;
            }
        }  
    }





};






}