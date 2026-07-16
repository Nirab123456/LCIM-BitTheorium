
#pragma once 
#include <array>
#include <utility>
#include "ConstructorsAndCarriersOfAPC.hpp"

namespace PredictedAdaptedEncoding
{


struct HashIdConstructror
{
    static constexpr uint64_t GROUP_IDX_BIT_BOUNDRY = 16u;
    static constexpr uint64_t GROUP_SEQUENTIAL_INDEX_MASK = UINT32_MAX;
    static constexpr uint64_t GROUP_PREFIX_MASK = UINT32_MAX;

    /// @brief VALIDATES THE RAW ID 
    static constexpr bool IsValidAPCId(uint64_t value) noexcept
    {
        return value != UNSIGNED_ZERO && value < FABRIC_CELL_SENTINAL;
    }

    static constexpr bool IsValidAPCSlotIdx(uint64_t slot_idx) noexcept
    {
        return slot_idx < FABRIC_CELL_SENTINAL - 1;
    }

    static constexpr bool IsValidHashHandle(uint64_t handle) noexcept
    {
        return handle > UNSIGNED_ZERO && handle < FABRIC_CELL_SENTINAL;
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
        if (IsValidHashHandle(handler))
        {
            return handler - 1;
        }
        return FABRIC_CELL_SENTINAL;
    }

    /// @brief CREATS: HASH KEY: Based On a Desired SHARED / LOGICAL Group ID
    /// @param sequential_idx_of_desired_id SEQUENTIAL IDX < UINT16_MAX - 1
    /// @return IF INVALID: UINT64_MAX
    static constexpr uint64_t MakeGroupAccessKey(uint64_t group_id, uint16_t sequential_idx_of_desired_id) noexcept
    {
        if (
            group_id == UNSIGNED_ZERO ||
            group_id > GROUP_SEQUENTIAL_INDEX_MASK ||
            !APCDataStructure::IsThisIndexValidForAPC(sequential_idx_of_desired_id)
        )
        {
            return FABRIC_CELL_SENTINAL;
        }

        const uint64_t key = ((group_id & GROUP_PREFIX_MASK) << GROUP_IDX_BIT_BOUNDRY) | (sequential_idx_of_desired_id & GROUP_SEQUENTIAL_INDEX_MASK);

        return IsValidAPCId(key) ? key : FABRIC_CELL_SENTINAL;
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
        
        return static_cast<uint32_t>((group_key >> GROUP_IDX_BIT_BOUNDRY) & GROUP_PREFIX_MASK);
    }

    /// @brief Get 16 Bit Sequential Linked Idx From Key
    /// @param group_key Raw Key
    /// @return if Key VALID: -> 16 BIT SEQUENTIAL IDX / std::nullopt
    static constexpr std::optional<uint16_t> GetSeqIndexOfAHashKey(uint64_t group_key) noexcept
    {
        if (!IsValidAPCId(group_key))
        {
            return std::nullopt;
        }

        return static_cast<uint16_t>(group_key & GROUP_SEQUENTIAL_INDEX_MASK);
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
            x += 0x9E3779B97F4A7C15ull;
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
            random_seed ^= 0xD6E8FEB86659FD93ull;
        }

        for (uint32_t attempt = 0; attempt < 8u; attempt++)
        {
            random_seed = SplitMix64(random_seed);

            if (!IsValidAPCId(random_seed))
            {
                return random_seed;
            }
        }
        
        const uint64_t fallback = SplitMix64(global_counter.fetch_add(1u, std::memory_order_acq_rel));

        return IsValidAPCId(fallback) ? fallback : FABRIC_CELL_SENTINAL;

    }

};

struct AxisConstructor
{
    enum class BidirectionalAxis : uint8_t
    {
        HORIZONTAL_SHARED = 1,
        VARTICAL_LOGICAL = 2
    };

    struct AxisConstructionMap
    {
        FabricTableSegmentClasses HashTable{FabricTableSegmentClasses::NULLNAN};
        HeaderIdentifierOfAPC CountTarget{HeaderIdentifierOfAPC::EOF_APC_HEADER};
        HeaderIdentifierOfAPC PreviousTarget{HeaderIdentifierOfAPC::EOF_APC_HEADER};
        HeaderIdentifierOfAPC NextTarget{HeaderIdentifierOfAPC::EOF_APC_HEADER};
        bool IsValid = false;
    };
    static_assert(sizeof(AxisConstructionMap) <= sizeof(uint64_t));

    static constexpr AxisConstructionMap ConstructAxisMap(BidirectionalAxis desired_axis) noexcept
    {
        if (desired_axis == BidirectionalAxis::HORIZONTAL_SHARED)
        {
            return AxisConstructionMap{
                FabricTableSegmentClasses::SHARED_HASH,
                HeaderIdentifierOfAPC::TOTAL_HORIZONTAL_COUNT_S,
                HeaderIdentifierOfAPC::PREVIOUS_HORIZONTAL_HANDLE,
                HeaderIdentifierOfAPC::NEXT_HORIZONTAL_HANDLE,
                true
            };
        }

        return AxisConstructionMap{
            FabricTableSegmentClasses::LOGICAL_HASH,
            HeaderIdentifierOfAPC::TOTAL_VERTICAL_COUNT_L,
            HeaderIdentifierOfAPC::PREVIOUS_VERTICAL_HANDLE,
            HeaderIdentifierOfAPC::NEXT_VERTICAL_HANDLE,
            true
        };
    }


    static constexpr bool IsHorizontalSharedAxis(BidirectionalAxis desired_axis) noexcept
    {
        if (desired_axis == BidirectionalAxis::HORIZONTAL_SHARED)
        {
            return true;
        }
        return false;
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
            HashIdConstructror::IsValidHashHandle(
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
        
        if (desired_axis == BidirectionalAxis::HORIZONTAL_SHARED)
        {
            if (a_runtime_identity.HorizontalSharedState == APCIdentityDef::DEFAULT_ASSIGNMENT)
            {
                a_runtime_identity.SharedGroupId = HashIdConstructror::MakeGroupAccessKey(a_runtime_identity.BranchID, UNSIGNED_ZERO);
                return APCIdentityDef::DEFAULT_ASSIGNMENT;
            }

            switch (a_runtime_identity.HorizontalSharedState)
            {
            case APCIdentityDef::DEFAULT_ASSIGNMENT:
                a_runtime_identity.SharedGroupId = HashIdConstructror::MakeGroupAccessKey(a_runtime_identity.BranchID, UNSIGNED_ZERO);
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
                a_runtime_identity.LogicalGroupId = HashIdConstructror::MakeGroupAccessKey(a_runtime_identity.BranchID, UNSIGNED_ZERO);
                return APCIdentityDef::DEFAULT_ASSIGNMENT;
            }

            switch (a_runtime_identity.VarticalLogicState)
            {
            case APCIdentityDef::DEFAULT_ASSIGNMENT:
                a_runtime_identity.LogicalGroupId = HashIdConstructror::MakeGroupAccessKey(a_runtime_identity.BranchID, UNSIGNED_ZERO);
                return APCIdentityDef::DEFAULT_ASSIGNMENT;

            case APCIdentityDef::NULL_USER_INSTRUCTION:
                return APCIdentityDef::NULL_USER_INSTRUCTION;
                        
            default:
                if (!HashIdConstructror::IsValidAPCId(a_runtime_identity.LogicalGroupId))
                {
                    return APCIdentityDef::UNASSIGNED_UNUSED_NANNULL;
                }
                return a_runtime_identity.HorizontalSharedState;
            }
        }  
    }
};






}