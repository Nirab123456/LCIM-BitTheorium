#pragma once
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"
#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace BidirectionalInMemGraph
{

    std::optional<DescriptionOfAPC::StateOfAPC> ConstructAPC::ReadValidAPCRangeInternally__(
        uint32_t apc_slot_idx,
        DescriptionOfAPC::InternalAPCRange& range_return
    ) noexcept
    {
        DSA::SingleAPCDescriptionCellBuffer desc_buffer{};
        range_return = DSA::InternalAPCRange{};

        if (
            !ReadACompleateAPCDescriptorBuffer_(apc_slot_idx, desc_buffer)
        )
        {
            return std::nullopt;
        }

        const DSA::DescriptionStateLockValues state = DSA::GetDescriptionFile(desc_buffer[static_cast<uint8_t>(DSA::DescriptionIndexing::ID_STATE_CONCURRENT)]);
        if (
            !state.IsValid ||
            (
                state.StateOfTheAPC != DSA::StateOfAPC::LIVE &&
                state.StateOfTheAPC != DSA::StateOfAPC::HAULTED
            )
        )
        {
            return state.StateOfTheAPC;
        }

        range_return.BeginIndex = desc_buffer[static_cast<uint8_t>(DSA::DescriptionIndexing::APC_SEGMENTPOOL_BEGAIN_SLAB)];
        range_return.EndIndex = desc_buffer[static_cast<uint8_t>(DSA::DescriptionIndexing::APC_SEGMENTPOOL_END_SLAB)];

        if (
            range_return.EndIndex > range_return.BeginIndex &&
            range_return.BeginIndex >= SegmentPoolBegin_ &&
            range_return.EndIndex <= SegmentPoolEnd_
        )
        {
            range_return.IsValid = true;
        }
        
        return state.StateOfTheAPC;
    }


    std::optional<DescriptionOfAPC::StateOfAPC> ConstructAPC::ReadIdentityBufferOfAPC(
        uint32_t apc_slot,
        IAB::BufferOfAPCIdentity& identity,
        uint32_t max_tries
    ) noexcept
    {
        DSA::InternalAPCRange range_of_apc_sagmant_pool{};
        std::optional<DSA::StateOfAPC> current_state = ReadValidAPCRangeInternally__(
            apc_slot,
            range_of_apc_sagmant_pool
        );

        if (
            !range_of_apc_sagmant_pool.IsValid ||
            !current_state.has_value()
        )
        {
            return std::nullopt;
        }

        const uint8_t internal_st_lock_idx = static_cast<uint8_t>(HeaderIdentifierOfAPC::GRAPH_MUTATION_AND_LOCK);
        const size_t st_lock_idx = range_of_apc_sagmant_pool.BeginIndex + internal_st_lock_idx;
        uint64_t begin_st_lock_raw = FABRIC_CELL_SENTINAL;

        for (size_t i = 0; i < max_tries; i++)
        {
            if (!AtomicallyLoadReadAUnit(st_lock_idx, begin_st_lock_raw))
            {
                return std::nullopt;
            }

            if (
                !ReadASnapShotFromSlab(
                    st_lock_idx,
                    APCDataStructure::TotalIdentityUnitCount(),
                    identity.data(),
                    true
                )
            )
            {
                return std::nullopt;
            }

            if (begin_st_lock_raw != identity[IAB::GetBufferIdxFromIdentityUnit(HeaderIdentifierOfAPC::GRAPH_MUTATION_AND_LOCK).value()])
            {
                continue;
            }

            if (!IAB::ValidateIdentityBuffer(identity))
            {
                return std::nullopt;
            }
            
            return current_state;
        }
        
        return std::nullopt;
    }

    bool ConstructAPC::WriteAcquiredAxis_(
        uint32_t apc_slot,
        const IAB::BufferOfAPCIdentity& identity,
        IAB::BidirectionalAxis axis
    ) noexcept
    {
        DSA::InternalAPCRange range_of_apc_sagmant_pool{};
        std::optional<DSA::StateOfAPC> current_state = ReadValidAPCRangeInternally__(
            apc_slot,
            range_of_apc_sagmant_pool
        );
        IAB::GraphMutationValues current_lock{};
        const IAB::AxisConstructionMap map = IAB::ConstructAxisMap(axis);

        if (
            !ReadGraphMutationFlags(apc_slot, current_lock) ||
            !IAB::DoseCurrentFlagsAllowsThisAxisMutation(current_lock.Flags, axis)
        )
        {
            return false;
        }

        AtomicallyStoreU64Fab(
            range_of_apc_sagmant_pool.BeginIndex + static_cast<uint8_t>(map.PreviousSibling),
            identity[IAB::GetBufferIdxFromIdentityUnit(map.PreviousSibling).value()],
            std::memory_order_relaxed
        );

        AtomicallyStoreU64Fab(
            range_of_apc_sagmant_pool.BeginIndex + static_cast<uint8_t>(map.NextSibling),
            identity[IAB::GetBufferIdxFromIdentityUnit(map.NextSibling).value()],
            std::memory_order_relaxed
        );

        AtomicallyStoreU64Fab(
            range_of_apc_sagmant_pool.BeginIndex + static_cast<uint8_t>(map.InheritedEgdeTableIdx),
            identity[IAB::GetBufferIdxFromIdentityUnit(map.InheritedEgdeTableIdx).value()],
            std::memory_order_relaxed
        );

        AtomicallyStoreU64Fab(
            range_of_apc_sagmant_pool.BeginIndex + static_cast<uint8_t>(map.OwnedEgdeTableIdx),
            identity[IAB::GetBufferIdxFromIdentityUnit(map.OwnedEgdeTableIdx).value()],
            std::memory_order_relaxed
        );

        AtomicallyStoreU64Fab(
            range_of_apc_sagmant_pool.BeginIndex + static_cast<uint8_t>(map.RootOwnedChild),
            identity[IAB::GetBufferIdxFromIdentityUnit(map.RootOwnedChild).value()],
            std::memory_order_relaxed
        );

        return true;
    }

    bool ConstructAPC::ReadGraphMutationFlags(
        uint32_t slot_idx,
        IAB::GraphMutationValues& values
    ) noexcept
    {
        DSA::InternalAPCRange range_of_apc_sagmant_pool{};
        std::optional<DSA::StateOfAPC> current_state = ReadValidAPCRangeInternally__(
            slot_idx,
            range_of_apc_sagmant_pool
        );
        const size_t identity_begin = range_of_apc_sagmant_pool.BeginIndex +
            static_cast<uint8_t>(HeaderIdentifierOfAPC::GRAPH_MUTATION_AND_LOCK);

        uint64_t mutation_lock = FABRIC_CELL_SENTINAL;

        if (
            !range_of_apc_sagmant_pool.IsValid ||
            !current_state.has_value() ||
            !AtomicallyLoadReadAUnit(identity_begin, mutation_lock)
        )
        {
            return false;
        }
        return IAB::ExtractGraphMutationValues(mutation_lock, values);
    }


    std::optional<uint64_t> ConstructAPC::AcquireGraphMutationFlag_(
        uint32_t apc_slot_idx,
        IAB::MemGraphFlag desired_state,
        uint32_t max_tries 
    ) noexcept
    {
        DSA::InternalAPCRange range_of_apc{};

        std::optional<DescriptionOfAPC::StateOfAPC> dsc_state = ReadValidAPCRangeInternally__(
            apc_slot_idx,
            range_of_apc
        );

        if (
            desired_state == IAB::MemGraphFlag::LIVE ||
            !dsc_state.has_value() ||
            dsc_state != DSA::StateOfAPC::LIVE ||
            !range_of_apc.IsValid
        )
        {
            return std::nullopt;
        }

        const size_t gmv_idx = range_of_apc.BeginIndex + static_cast<uint8_t>(HeaderIdentifierOfAPC::GRAPH_MUTATION_AND_LOCK);
        uint64_t gmv_raw = FABRIC_CELL_SENTINAL;
        IAB::GraphMutationValues gmv_values{};

        for (size_t i = 0; i < max_tries; i++)
        {
            if (
                !AtomicallyLoadReadAUnit(gmv_idx, gmv_raw) ||
                !APCDataStructure::IsValidFabricUnit(gmv_raw) ||
                !IAB::ExtractGraphMutationValues(gmv_raw, gmv_values)

            )
            {
                return std::nullopt;
            }

            if (
                IAB::HasThisGraphMutationFlag(gmv_values.Flags, desired_state) ||
                IAB::HasThisGraphMutationFlag(gmv_values.Flags, IAB::MemGraphFlag::READ_ONLY) ||
                (
                    desired_state == IAB::MemGraphFlag::READ_ONLY &&
                    !IAB::IsIdentityGraphUnlocked(gmv_values.Flags)
                )
            )
            {
                continue;
            }
            IAB::GraphMutationValues updated_gmv{};

            if (desired_state == IAB::MemGraphFlag::READ_ONLY)
            {
                updated_gmv.Flags = IAB::SetGraphFlags(gmv_values.Flags, desired_state);
                updated_gmv.SeqLock = gmv_values.SeqLock + 2u;
            }
            //new insert
            else if (IAB::IsIdentityGraphUnlocked(gmv_values.Flags))
            {
                updated_gmv.Flags = IAB::SetGraphFlags(gmv_values.Flags, desired_state);
                updated_gmv.SeqLock = gmv_values.SeqLock + 1u;
            }
            // already has a flag
            else
            {
                updated_gmv.Flags = IAB::SetGraphFlags(gmv_values.Flags, desired_state);
                updated_gmv.SeqLock = gmv_values.SeqLock + 2u;
            }

            const uint64_t updated_gmv_raw = IAB::MakeGraphMutationRaw(updated_gmv);
            
            if (
                !IAB::IsValidGraphMutationState(updated_gmv) ||
                !APCDataStructure::IsValidFabricUnit(updated_gmv_raw)
            )
            {
                return std::nullopt;
            }
            
            if (!CompareExchangeStrongFromFabric(
                gmv_idx,
                gmv_raw,
                updated_gmv_raw
            ))
            {
                continue;
            }
            
            return gmv_raw;
        }
    
        return std::nullopt;
    }

    bool ConstructAPC::ReleseGraphMutationFlag_(
        uint32_t apc_slot,
        IAB::BidirectionalAxis axis,
        uint32_t max_tries
    ) noexcept
    {
        DSA::InternalAPCRange range_of_apc_sagmant_pool{};
        std::optional<DSA::StateOfAPC> current_state = ReadValidAPCRangeInternally__(
            apc_slot,
            range_of_apc_sagmant_pool
        );
        const IAB::MemGraphFlag axis_flag = (axis == IAB::BidirectionalAxis::HORIZONTAL) ? 
            IAB::MemGraphFlag::HORIZONTAL_LOCK : IAB::MemGraphFlag::VERTICAL_LOCK;

        if (
            !current_state.has_value() || 
            current_state.value() != DSA::StateOfAPC::LIVE || 
            !range_of_apc_sagmant_pool.IsValid
        )
        {
            return false;
        }

        const size_t lock_idx = range_of_apc_sagmant_pool.BeginIndex + static_cast<uint8_t>(HeaderIdentifierOfAPC::GRAPH_MUTATION_AND_LOCK);

        uint64_t observed = FABRIC_CELL_SENTINAL;
        IAB::GraphMutationValues current_values{};

        for (size_t i = 0; i < max_tries; i++)
        {
            if (
                !AtomicallyLoadReadAUnit(lock_idx, observed) ||
                !IAB::ExtractGraphMutationValues(observed, current_values) ||
                !IAB::HasThisGraphMutationFlag(current_values.Flags, axis_flag)
            )
            {
                return false;
            }

            IAB:: GraphMutationValues desired{};
            desired.Flags = IAB::ClearThisFlag(current_values.Flags, axis_flag);

            if (IAB::IsIdentityGraphUnlocked(desired.Flags))
            {
                desired.SeqLock = current_values.SeqLock + 1u;
            }
            else
            {
                desired.SeqLock = current_values.SeqLock + 2u;
            }
            
            const uint64_t desired_raw = IAB::MakeGraphMutationRaw(desired);
            if (
                !IAB::IsValidGraphMutationState(desired) ||
                !APCDataStructure::IsValidFabricUnit(desired_raw)
            )
            {
                return false;
            }

            uint64_t expected = observed;

            if (
                !CompareExchangeStrongFromFabric(
                    lock_idx,
                    expected,
                    desired_raw
                )
            )
            {
                continue;
            }
            
            return true;
        }
        return false;
    }


    // bool ConstructAPC::CreateAPCInternal(
    //     uint64_t apc_idx,
    //     bool wants_horizontal_root,
    //     bool wants_vertical_root,
    //     const LBO::
    //         LayoutSpanAndPercentageCarrier&
    //             layout,
    //     const SD::
    //         InitialRegionalDtypeConf&
    //             dtype,
    //     const SD::
    //         InitialRegionalProtocol&
    //             protocol,
    //     uint8_t version
    // ) noexcept
    // {

    //     DSA::SingleAPCDescriptionCellBuffer description{};

    //     if (
    //         !FabricInitialized_.load(std::memory_order_acquire) ||
    //         !SlabBasePtr_ ||
    //         apc_idx >= CountOfAPC_ ||
    //         !APCDataStructure::IsCapacityOfAPCValid(PerAPCRuntimeCellCount_) ||
    //         !ReadACompleateAPCDescriptorBuffer_(apc_idx, description)
    //     )
    //     {
    //         return false;
    //     }

    //     const DSA::DescriptionStateLockValues dsc_st_lock = DSA::GetDescriptionFile(description[static_cast<uint8_t>(DSA::DescriptionIndexing::ID_STATE_CONCURRENT)]);

    //     if (
    //         !
    //     )
    //     {
    //         /* code */
    //     }
        


        
    // }



}