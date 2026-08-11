#pragma once
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"
#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace BidirectionalInMemGraph
{

    std::optional<StateOfAPC> ConstructAPCIdentity::ReadIdentityBufferOfAPC(
        uint32_t apc_slot,
        IAB::BufferOfAPCIdentity& identity,
        uint32_t max_tries
    ) noexcept
    {
        const RangeOfAPC range_of_apc_sagmant_pool = GetSegmentPoolRange(apc_slot);
        DSA::DescriptionLockValues current_apc_state = ReadAPCStateAtomically_(apc_slot);

        if (
            !range_of_apc_sagmant_pool.IsValid ||
            !current_apc_state.IsValid
        )
        {
            return std::nullopt;
        }

        const uint8_t internal_st_lock_idx = static_cast<uint8_t>(HeaderIdentifierOfAPC::GRAPH_MUTATION_AND_LOCK);
        const size_t st_lock_idx = range_of_apc_sagmant_pool.BeginIndex + internal_st_lock_idx;
        uint64_t after_read_lock = FABRIC_CELL_SENTINAL;
        IAB::GraphMutationValues begin_values{};

        for (size_t i = 0; i < max_tries; i++)
        {
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

            if (!IAB::ValidateIdentityBuffer(identity))
            {
                continue;
            }

            if (!AtomicallyLoadReadAUnit(st_lock_idx, after_read_lock))
            {
                return std::nullopt;
            }
            
            if (
                after_read_lock != identity[IAB::GetBufferIdxFromIdentityUnit(HeaderIdentifierOfAPC::GRAPH_MUTATION_AND_LOCK).value()]
            )
            {
                continue;
            }
            
            return current_apc_state.StateOfTheAPC;
        }
        
        return std::nullopt;
    }

    bool ConstructAPCIdentity::WriteAcquiredAxis_(
        uint32_t apc_slot,
        const IAB::BufferOfAPCIdentity& identity,
        IAB::BidirectionalAxis axis
    ) noexcept
    {
        RangeOfAPC range_of_apc_sagmant_pool = GetSegmentPoolRange(apc_slot);
        DSA::DescriptionLockValues current_apc_state = ReadAPCStateAtomically_(apc_slot);
        IAB::GraphMutationValues current_lock{};
        const IAB::AxisConstructionMap map = IAB::ConstructAxisMap(axis);

        if (
            !current_apc_state.IsValid ||
            current_apc_state.StateOfTheAPC != StateOfAPC::LIVE ||
            !range_of_apc_sagmant_pool.IsValid ||
            !IAB::ValidateIdentityBuffer(identity) ||
            IAB::ValueOfAnIdentityFromBuffer(
                identity,
                HeaderIdentifierOfAPC::APC_SLOT_IDX
            ) != apc_slot ||
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

    bool ConstructAPCIdentity::ReadGraphMutationFlags(
        uint32_t slot_idx,
        IAB::GraphMutationValues& values
    ) noexcept
    {
        const RangeOfAPC range_of_apc_sagmant_pool = GetSegmentPoolRange(slot_idx);

        const size_t identity_begin = range_of_apc_sagmant_pool.BeginIndex +
            static_cast<uint8_t>(HeaderIdentifierOfAPC::GRAPH_MUTATION_AND_LOCK);

        uint64_t mutation_lock = FABRIC_CELL_SENTINAL;

        if (
            !range_of_apc_sagmant_pool.IsValid ||
            !AtomicallyLoadReadAUnit(identity_begin, mutation_lock)
        )
        {
            return false;
        }
        return IAB::ExtractGraphMutationValues(mutation_lock, values);
    }


    std::optional<uint64_t> ConstructAPCIdentity::AcquireGraphMutationFlag_(
        uint32_t apc_slot_idx,
        IAB::MemGraphFlag desired_state,
        uint32_t max_tries 
    ) noexcept
    {
        const RangeOfAPC range_of_apc = GetSegmentPoolRange(apc_slot_idx);
        const DSA::DescriptionLockValues dsc_state = ReadAPCStateAtomically_(apc_slot_idx); 

        if (
            desired_state == IAB::MemGraphFlag::LIVE ||
            !dsc_state.IsValid ||
            dsc_state.StateOfTheAPC != StateOfAPC::LIVE ||
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
            IAB::GraphMutationValues updated_gmv = IAB::UpdateSeqkBasedOnDesiredLock(gmv_values, desired_state);

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

    bool ConstructAPCIdentity::ReleseGraphMutationFlag_(
        uint32_t apc_slot,
        IAB::BidirectionalAxis axis,
        uint32_t max_tries
    ) noexcept
    {
        const RangeOfAPC range_of_apc_sagmant_pool = GetSegmentPoolRange(apc_slot);
        const DSA::DescriptionLockValues current_apc_state = ReadAPCStateAtomically_(apc_slot); 

        const IAB::MemGraphFlag axis_flag = (axis == IAB::BidirectionalAxis::HORIZONTAL) ? 
            IAB::MemGraphFlag::HORIZONTAL_LOCK : IAB::MemGraphFlag::VERTICAL_LOCK;

        if (
            !current_apc_state.IsValid || 
            current_apc_state.StateOfTheAPC != StateOfAPC::LIVE || 
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

            IAB:: GraphMutationValues desired = current_values;
            desired.Flags = IAB::ClearThisFlag(current_values.Flags, axis_flag);

            IAB::UpdateCounterOnDesiredState(desired, axis_flag);
            
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

    bool ConstructAPCIdentity::AttachValidIdentity(uint32_t apc_idx) noexcept
    {

        DSA::SingleAPCDescriptionCellBuffer description_buffer{};
        IAB::BufferOfAPCIdentity identity_buffer{};

        if (
            apc_idx > CountOfAPC_ ||
            !ReadACompleateAPCDescriptorBuffer_(apc_idx, description_buffer)
        )
        {
            return false;
        }

        const DSA::DescriptionLockValues dsc_st_lock = DSA::GetDescriptionFile(
            description_buffer[static_cast<uint8_t>(DSA::DescriptionIndexing::ID_STATE_CONCURRENT)]
        );

        if (
            !dsc_st_lock.IsValid ||
            dsc_st_lock.StateOfTheAPC != StateOfAPC::RESERVED ||
            !DSA::BuildIdentityBufferFromDescriptionBuffer(
                description_buffer,
                identity_buffer
            )
        )
        {
            return false;
        }

        RangeOfAPC range = GetSegmentPoolRange(apc_idx);
        const DSA::DescriptionLockValues dsc_lock_files = DSA::GetDescriptionFile(description_buffer[static_cast<uint8_t>(DSA::DescriptionIndexing::ID_STATE_CONCURRENT)]);
        const uint64_t begin_idx = description_buffer[static_cast<uint8_t>(DSA::DescriptionIndexing::APC_SEGMENTPOOL_BEGAIN_SLAB)];
        const uint64_t end_idx = description_buffer[static_cast<uint8_t>(DSA::DescriptionIndexing::APC_SEGMENTPOOL_END_SLAB)];

        if (
            !range.IsValid ||
            dsc_lock_files.IsValid ||
            dsc_lock_files.StateOfTheAPC != StateOfAPC::RESERVED ||
            range.BeginIndex != begin_idx ||
            range.EndIndex != end_idx
        )
        {
            return false;
        }

        return
            ForceNxLenMemCopy(
                range.BeginIndex + static_cast<uint8_t>(HeaderIdentifierOfAPC::GRAPH_MUTATION_AND_LOCK),
                APCDataStructure::TotalIdentityUnitCount(),
                identity_buffer.data()
            );
    }

    bool ConstructAPCIdentity::InitiateRootAxis(
        uint32_t apc_slot,
        IAB::BidirectionalAxis axis
    ) noexcept
    {
        if (apc_slot >= CountOfAPC_ )
        {
            return false;
        }

        IAB::AxisConstructionMap map = IAB::ConstructAxisMap(axis);

        IAB::MemGraphFlag axis_lock = axis == IAB::BidirectionalAxis::HORIZONTAL ?
            IAB::MemGraphFlag::HORIZONTAL_LOCK : IAB::MemGraphFlag::VERTICAL_LOCK;

        EdgeBuilder::EdgeData reserved_edge{};

        const std::optional<EdgeBuilder::EdgeStatus> edge_status = ReadEdgeData_(
            map.EdgeTable,
            apc_slot,
            reserved_edge
        );

        if (
            !edge_status.has_value() ||
            edge_status.value() != EdgeBuilder::EdgeStatus::RESERVED ||
            !reserved_edge.IsValid ||
            reserved_edge.Root != apc_slot
        )
        {
            return false;
        }

        auto RevertEdgeToFree____ = [&]()
        {
            EdgeBuilder::EdgeData free_edge{};
            if (
                !EdgeBuilder::BuildFreeEdgeTable(map.EdgeTable, apc_slot, free_edge)
            )
            {
                return false;
            }
            return PublishReservedEdge_(free_edge, apc_slot);
        };

        if (
            !AcquireGraphMutationFlag_(apc_slot, axis_lock).has_value()
        )
        {
            RevertEdgeToFree____();
            return false;
        }

        bool axis_locked = true;

        auto ReleseAxis___ = [&]()
        {
            if (!axis_locked)
            {
                return true;
            }
            const bool relesed = ReleseGraphMutationFlag_(apc_slot, axis);
            if (relesed)
            {
                axis_locked = false;
            }
            return relesed;
        };

        IAB::BufferOfAPCIdentity identity_buffer{};
        const std::optional<StateOfAPC> current_apc_state = ReadIdentityBufferOfAPC(apc_slot, identity_buffer);

        if (
            !current_apc_state.has_value() ||
            current_apc_state.value() != StateOfAPC::LIVE ||
            !IAB::IsOwnedAxisDisabled(identity_buffer, axis)
        )
        {
            ReleseAxis___();
            RevertEdgeToFree____();
            return false;
        }

        const IAB::BufferOfAPCIdentity before_identity = identity_buffer;

        EdgeBuilder::EdgeData desired_edge{};
        if (!EdgeBuilder::InstallOwnedRoot(
            identity_buffer,
            axis,
            apc_slot,
            desired_edge
        ))
        {
            ReleseAxis___();
            RevertEdgeToFree____();
            return false;
        }
        
        if (!WriteAcquiredAxis_(apc_slot, identity_buffer, axis))
        {
            ReleseAxis___();
            RevertEdgeToFree____();
            return false;
        }

        if (!PublishReservedEdge_(desired_edge, apc_slot))
        {
            WriteAcquiredAxis_(apc_slot, identity_buffer, axis);
            RevertEdgeToFree____();
            ReleseAxis___();
            return false;
        }
        
        return ReleseAxis___();
    }


}