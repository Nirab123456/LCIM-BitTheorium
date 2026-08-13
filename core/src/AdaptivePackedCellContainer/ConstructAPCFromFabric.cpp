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
            !IsLiveSateOfAPC(current_apc_state.StateOfTheAPC) ||
            !current_apc_state.IsValid ||
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
            !IsLiveSateOfAPC(dsc_state.StateOfTheAPC) ||
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
        const IAB::MemGraphFlag axis_flag = IAB::GetMemGFlagFromAxis(axis);

        if (
            !current_apc_state.IsValid || 
            !IsLiveSateOfAPC(current_apc_state.StateOfTheAPC) ||
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
            apc_idx >= CountOfAPC_ ||
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
            !dsc_lock_files.IsValid ||
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

        IAB::MemGraphFlag axis_lock = IAB::GetMemGFlagFromAxis(axis);

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
            !IsLiveSateOfAPC(current_apc_state) ||
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

    bool ConstructAPCIdentity::LinkTwoAPC(
        uint32_t predessor_idx,
        uint32_t child_idx,
        IAB::BidirectionalAxis axis,
        IAB::DescOfInharitance inharitance,
        uint32_t internal_max_tries
    ) noexcept
    {
        if (
            predessor_idx == child_idx ||
            predessor_idx >= CountOfAPC_ ||
            child_idx >= CountOfAPC_
        )
        {
            return false;
        }

        const IAB::AxisConstructionMap map = IAB::ConstructAxisMap(axis);
        const IAB::MemGraphFlag axis_flag = IAB::GetMemGFlagFromAxis(axis);

        IAB::BufferOfAPCIdentity predessor_buffer{};
        std::optional<StateOfAPC> state_of_predessor = ReadIdentityBufferOfAPC(predessor_idx, predessor_buffer, internal_max_tries);

        if (!IsLiveSateOfAPC(state_of_predessor))
        {
            return false;
        }

        uint64_t owned_edge_raw = FABRIC_CELL_SENTINAL;
        if (inharitance == IAB::DescOfInharitance::FIRST_CHILD)
        {
            owned_edge_raw = IAB::ValueOfAnIdentityFromBuffer(
                predessor_buffer,
                map.OwnedEgdeTableIdx
            );
        }
        else
        {
            owned_edge_raw = IAB::ValueOfAnIdentityFromBuffer(
                predessor_buffer,
                map.InheritedEgdeTableIdx
            );
        }


        uint32_t roots_edge_idx = static_cast<uint32_t>(owned_edge_raw);
        EdgeBuilder::EdgeData owner_edge_before{};

        if (
            !APCDataStructure::IsValid32BitAPCUnit(owned_edge_raw) ||
            !ReserveAnEdge_(
                map.EdgeTable,
                static_cast<uint32_t>(owned_edge_raw),
                &owner_edge_before,
                std::nullopt,
                internal_max_tries
            )
        )
        {
            return false;
        }
        
        if (!owner_edge_before.IsValid)
        {
            PublishReservedEdge_(owner_edge_before, roots_edge_idx);
            return false;
        }

        const uint32_t first_lock = std::min(predessor_idx, child_idx);
        const uint32_t second_lock = std::max(predessor_idx, child_idx);

        if (
            !AcquireGraphMutationFlag_(first_lock, axis_flag, internal_max_tries).has_value()
        )
        {
            PublishReservedEdge_(owner_edge_before, roots_edge_idx);
            return false;
        }

        if (
            !AcquireGraphMutationFlag_(second_lock, axis_flag, internal_max_tries).has_value()
        )
        {
            ReleseGraphMutationFlag_(first_lock, axis, internal_max_tries);
            PublishReservedEdge_(owner_edge_before, roots_edge_idx);
            return false;
        }
        
        auto ReleseBothAxisLocks___ = [&]() noexcept -> void
        {
            ReleseGraphMutationFlag_(first_lock, axis, internal_max_tries);
            ReleseGraphMutationFlag_(second_lock, axis, internal_max_tries);
        };

        auto ReleseAxisLockOwnerEdge___ = [&]() noexcept -> void
        {
            ReleseBothAxisLocks___();
            PublishReservedEdge_(owner_edge_before, roots_edge_idx);
        };


        IAB::BufferOfAPCIdentity child_buffer{};

        const std::optional<StateOfAPC> predessor_state = ReadIdentityBufferOfAPC(predessor_idx, predessor_buffer);
        const std::optional<StateOfAPC> child_state = ReadIdentityBufferOfAPC(child_idx, child_buffer);


        if (
            !IsLiveSateOfAPC(predessor_state) ||
            !IsLiveSateOfAPC(child_state) ||
            !IAB::IsInheritedAxisDisabled(child_buffer, axis)
        )
        {
            ReleseAxisLockOwnerEdge___();
            return false;
        }
        
        uint64_t verified_owned_edge = FABRIC_CELL_SENTINAL;
        if (inharitance == IAB::DescOfInharitance::FIRST_CHILD)
        {
            verified_owned_edge = IAB::ValueOfAnIdentityFromBuffer(predessor_buffer, map.OwnedEgdeTableIdx);
        }
        else
        {
            verified_owned_edge = IAB::ValueOfAnIdentityFromBuffer(predessor_buffer, map.InheritedEgdeTableIdx);
        }

        if (verified_owned_edge != roots_edge_idx)
        {
            ReleseAxisLockOwnerEdge___();
            return false;
        }
        
        const IAB::BufferOfAPCIdentity predessor_before = predessor_buffer;
        const IAB::BufferOfAPCIdentity child_before = child_buffer;

        const uint64_t child_owned_edge_raw = IAB::ValueOfAnIdentityFromBuffer(child_buffer, map.OwnedEgdeTableIdx);
        
        bool child_owned_reserved = false;
        bool child_owned_published = false;

        uint32_t child_as_root_edge_idx = APCDataStructure::APC_INDEX_BOUND_SENTINAL;

        EdgeBuilder::EdgeData child_owned_before{};
        EdgeBuilder::EdgeData child_owned_work{};

        if (child_owned_edge_raw != FABRIC_CELL_SENTINAL)
        {
            if (!APCDataStructure::IsValid32BitAPCUnit(child_owned_edge_raw))
            {
                ReleseAxisLockOwnerEdge___();
                return false;
            }

            child_as_root_edge_idx = static_cast<uint32_t>((child_owned_edge_raw));

            if (
                child_as_root_edge_idx == roots_edge_idx ||
                !ReserveAnEdge_(
                    map.EdgeTable,
                    child_as_root_edge_idx,
                    &child_owned_before,
                    EdgeBuilder::EdgeStatus::LIVE,
                    internal_max_tries
                )
            )
            {
                ReleseAxisLockOwnerEdge___();
                return false;
            }
            
            child_owned_reserved = true;
            child_owned_work = child_owned_before;
        }
        
        EdgeBuilder::EdgeData owner_edge_work = owner_edge_before;
        if (!EdgeBuilder::PrepareInharitedAxis(
            predessor_buffer,
            child_buffer,
            axis,
            inharitance,
            roots_edge_idx,
            owner_edge_work,
            child_owned_reserved ? &child_owned_work : nullptr
        ))
        {
            if (child_owned_reserved)
            {
                PublishReservedEdge_(child_owned_before, child_as_root_edge_idx);
            }
            ReleseAxisLockOwnerEdge___();
            return false;
        }
        
        const bool predessor_written = WriteAcquiredAxis_(predessor_idx, predessor_buffer, axis);
        const bool child_written = WriteAcquiredAxis_(child_idx, child_buffer, axis);

        auto RestoreIdentityValues___ = [&]() noexcept -> void
        {
            if (predessor_written)
            {
                WriteAcquiredAxis_(predessor_idx, predessor_before, axis);
            }

            if (child_written)
            {
                WriteAcquiredAxis_(child_idx, child_before, axis);
            }
        };

        auto ReleseAll___ = [&]() noexcept -> void
        {
            RestoreIdentityValues___();
            if (child_owned_reserved)
            {
                PublishReservedEdge_(child_owned_before, child_as_root_edge_idx);
            }
            ReleseAxisLockOwnerEdge___();
        };

        if (
            !predessor_written ||
            !child_written
        )
        {
            ReleseAll___();
            return false;   
        }

        if (child_owned_reserved)
        {
            if (!PublishReservedEdge_(
                child_owned_work,
                child_as_root_edge_idx
            ))
            {
                ReleseAll___();
                return false;
            }
            child_owned_published = true;
        }

        if (!PublishReservedEdge_(owner_edge_work, roots_edge_idx))
        {
            RestoreIdentityValues___();
            if (child_owned_published)
            {
                if (
                    ReserveAnEdge_(map.EdgeTable, child_as_root_edge_idx, nullptr, std::nullopt, internal_max_tries)
                )
                {
                    PublishReservedEdge_(child_owned_before, child_as_root_edge_idx);
                }
            }
            else if (child_owned_reserved)
            {
                PublishReservedEdge_(child_owned_before, child_as_root_edge_idx);
            }
            ReleseAxisLockOwnerEdge___();
            return false;
        }
        
        return ReleseGraphMutationFlag_(first_lock, axis, internal_max_tries) &&
            ReleseGraphMutationFlag_(second_lock, axis, internal_max_tries);
    }





    bool ConstructAPCIdentity::UnlinkTwoAPC(
        uint32_t child_idx,
        IAB::BidirectionalAxis axis,
        uint32_t internal_max_tries
    ) noexcept
    {
        if (child_idx >= CountOfAPC_)
        {
            return false;
        }

        const IAB::AxisConstructionMap map = IAB::ConstructAxisMap(axis);
        const IAB::MemGraphFlag axis_flag = IAB::GetMemGFlagFromAxis(axis);

        IAB::BufferOfAPCIdentity child_buffer_idintity{};
        const std::optional<StateOfAPC> child_prob_state = ReadIdentityBufferOfAPC(child_idx, child_buffer_idintity);
        if (
            !IsLiveSateOfAPC(child_prob_state) ||
            IAB::IsInheritedAxisDisabled(child_buffer_idintity, axis)
        )
        {
            return false;
        }

        const uint32_t roots_edge_idx = static_cast<uint32_t>(IAB::ValueOfAnIdentityFromBuffer(
            child_buffer_idintity,
            map.InheritedEgdeTableIdx
        ));

        const uint32_t predessor_idx = static_cast<uint32_t>(IAB::ValueOfAnIdentityFromBuffer(
            child_buffer_idintity,
            map.PreviousSibling
        ));

        const uint64_t next_idx_of_same_parent = IAB::ValueOfAnIdentityFromBuffer(
            child_buffer_idintity,
            map.NextSibling
        );

        const uint64_t child_as_root_edge_idx = IAB::ValueOfAnIdentityFromBuffer(
            child_buffer_idintity,
            map.OwnedEgdeTableIdx
        );

        const bool child_has_own_root = child_as_root_edge_idx == FABRIC_CELL_SENTINAL ? false : true;

        if (
            !APCDataStructure::IsValid32BitAPCUnit(roots_edge_idx) ||
            !APCDataStructure::IsValid32BitAPCUnit(predessor_idx) ||
            predessor_idx >= CountOfAPC_ ||
            predessor_idx == child_idx ||
            (
                next_idx_of_same_parent != FABRIC_CELL_SENTINAL &&
                (
                    !APCDataStructure::IsValid32BitAPCUnit(next_idx_of_same_parent) ||
                    next_idx_of_same_parent >= CountOfAPC_ ||
                    next_idx_of_same_parent == child_idx ||
                    next_idx_of_same_parent == predessor_idx
                )
            ) ||
            (
                child_has_own_root && !APCDataStructure::IsValid32BitAPCUnit(child_as_root_edge_idx)
            )
        )
        {
            return false;
        }


        const bool has_next = next_idx_of_same_parent != FABRIC_CELL_SENTINAL;

        EdgeBuilder::EdgeData before_of_roots_edge_data{};
        EdgeBuilder::EdgeData before_childs_own_root_edge_data{};

        if (
            !ReserveAnEdge_(
                map.EdgeTable,
                roots_edge_idx,
                &before_of_roots_edge_data,
                EdgeBuilder::EdgeStatus::LIVE,
                internal_max_tries
            )
        )
        {
            return false;
        }
        

        if (
            child_has_own_root &&
            !ReserveAnEdge_(
                map.EdgeTable,
                static_cast<uint32_t>(child_as_root_edge_idx),
                &before_childs_own_root_edge_data,
                EdgeBuilder::EdgeStatus::LIVE,
                internal_max_tries
            )
        )
        {
            PublishReservedEdge_(before_of_roots_edge_data, roots_edge_idx);
            return false;
        }
        
        auto RevBothEdge___ = [&]() noexcept -> void
        {
            if (child_has_own_root)
            {
                PublishReservedEdge_(before_childs_own_root_edge_data, static_cast<uint32_t>(child_as_root_edge_idx));
            }
            PublishReservedEdge_(before_of_roots_edge_data, roots_edge_idx);
        };

        auto ValidEdge___ = [&](EdgeBuilder::EdgeData& edge_data___) noexcept -> bool
        {
            return 
                edge_data___.IsValid &&
                edge_data___.Status == EdgeBuilder::EdgeStatus::LIVE &&
                edge_data___.EdgeTable == map.EdgeTable;
        };

        if (
            !ValidEdge___(before_childs_own_root_edge_data) ||
            (
                child_has_own_root &&
                !ValidEdge___(before_of_roots_edge_data)
            ) ||
            before_of_roots_edge_data.OwnLinkCount == UNSIGNED_ZERO
        )
        {
            RevBothEdge___();
            return false;
        }
        
        if (
            before_of_roots_edge_data.End == child_idx &&
            has_next
        )
        {
            RevBothEdge___();
            return false;
        }

        static constexpr uint8_t CHANGABLE_TOTA_IDINTITY___ = 3;
        std::array<uint32_t, CHANGABLE_TOTA_IDINTITY___> lock_apcs_array{
            predessor_idx,
            child_idx,
            has_next ? static_cast<uint32_t>(next_idx_of_same_parent) : APCDataStructure::APC_INDEX_BOUND_SENTINAL
        };
        // We surely know UINT32_MAX will be last if other two are valid | 3 of them is valid
        std::sort(lock_apcs_array.begin(), lock_apcs_array.end());
        
        const uint8_t required_lock_count = has_next ? CHANGABLE_TOTA_IDINTITY___ : CHANGABLE_TOTA_IDINTITY___- 1;
        uint8_t locked_count = UNSIGNED_ZERO;

        auto ReleseGraphMutation___ = [&]() noexcept -> void
        {
            while (locked_count != UNSIGNED_ZERO)
            {
                --locked_count;
                ReleseGraphMutationFlag_(
                    lock_apcs_array[locked_count],
                    axis,
                    internal_max_tries
                );
            }
        };

        auto ReleseReservedGraphAndEdge___ = [&]() noexcept -> void
        {
            ReleseGraphMutation___();
            RevBothEdge___();
        };

        for (size_t i = 0; i < required_lock_count; i++)
        {
            if (!AcquireGraphMutationFlag_(
                lock_apcs_array[i],
                axis_flag,
                internal_max_tries
            ))
            {
                ReleseReservedGraphAndEdge___();
                return false;
            }
            ++locked_count;
        }

        IAB::BufferOfAPCIdentity predessor_buffer{};
        IAB::BufferOfAPCIdentity next_id_buffer{};

        const std::optional<StateOfAPC> predessor_state = ReadIdentityBufferOfAPC(
            predessor_idx,
            predessor_buffer,
            internal_max_tries
        );

        const std::optional<StateOfAPC> child_state = ReadIdentityBufferOfAPC(
            child_idx,
            child_buffer_idintity,
            internal_max_tries
        );

        std::optional<StateOfAPC> next_state = std::nullopt;

        if (has_next)
        {
            next_state = ReadIdentityBufferOfAPC(
                static_cast<uint32_t>(next_idx_of_same_parent),
                next_id_buffer,
                internal_max_tries
            );
        }

        const IAB::BufferOfAPCIdentity before_predessor_buffer = predessor_buffer;
        const IAB::BufferOfAPCIdentity before_child_buffer = child_buffer_idintity;
        const IAB::BufferOfAPCIdentity before_next_id_buffer = next_id_buffer;

        EdgeBuilder::EdgeData roots_edge_data =  before_of_roots_edge_data;
        EdgeBuilder::EdgeData childs_root_edge_data = before_childs_own_root_edge_data;

        if (
            !EdgeBuilder::PrepareForDetachmentOfInharitedAxis(
                predessor_buffer,
                child_buffer_idintity,
                has_next ? &next_id_buffer : nullptr,
                axis,
                roots_edge_idx,
                roots_edge_data,
                child_has_own_root ? &childs_root_edge_data : nullptr
            )
        )
        {
            ReleseReservedGraphAndEdge___();
            return false;
        }
        

        bool predessor_wrritten = false;
        bool next_wrritten = false;
        bool child_wrritten = false;

        auto RestoreIdinties___ = [&]() noexcept -> void
        {
            if (predessor_wrritten)
            {
                WriteAcquiredAxis_(predessor_idx, before_predessor_buffer, axis);
            }

            if (next_wrritten)
            {
                WriteAcquiredAxis_(
                    static_cast<uint32_t>(next_idx_of_same_parent),
                    before_next_id_buffer,
                    axis
                );
            }

            if (child_wrritten)
            {
                WriteAcquiredAxis_(
                    child_idx,
                    before_child_buffer,
                    axis
                );
            }
        };

        auto AbortMutation___ = [&]() noexcept -> void
        {
            RestoreIdinties___();
            ReleseReservedGraphAndEdge___();
        };

        if (WriteAcquiredAxis_(predessor_idx, predessor_buffer, axis))
        {
            predessor_wrritten = true;
        }
        else
        {
            ReleseReservedGraphAndEdge___();
            return false;
        }

        if (
            has_next &&
            WriteAcquiredAxis_(static_cast<uint32_t>(next_idx_of_same_parent), next_id_buffer, axis)
        )
        {
            next_wrritten = true;
        }

        if (has_next && !next_wrritten)
        {
            AbortMutation___();
            return false;
        }

        if (WriteAcquiredAxis_(child_idx, child_buffer_idintity, axis))
        {
            child_wrritten = true;
        }
        else
        {
            AbortMutation___();
            return false;
        }

        if (child_has_own_root)
        {
            if (
                !PublishReservedEdge_(
                    childs_root_edge_data,
                    static_cast<uint32_t>(child_as_root_edge_idx)
                )
            )
            {
                AbortMutation___();
                return false;
            }
        }

        if (!PublishReservedEdge_(roots_edge_data, roots_edge_idx))
        {
            AbortMutation___();
            return false;
        }

        return 
            ReleseGraphMutationFlag_(predessor_idx, axis, internal_max_tries) &&
            (
                has_next ? ReleseGraphMutationFlag_(
                    static_cast<uint32_t>(next_idx_of_same_parent), 
                    axis, internal_max_tries
                ) : true
            ) && 
            ReleseGraphMutationFlag_(child_idx, axis, internal_max_tries);
    }

}