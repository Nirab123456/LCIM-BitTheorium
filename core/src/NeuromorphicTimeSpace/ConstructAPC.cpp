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

        const DSA::DescriptorSaftyFiles state = DSA::GetDescriptionFile(desc_buffer[static_cast<uint8_t>(DSA::DescriptionIndexing::ID_STATE_CONCURRENT)]);
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
        IAB::BufferOfAPCIdentity& identity
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
        
        const size_t identity_begin = range_of_apc_sagmant_pool.BeginIndex +
            static_cast<uint8_t>(HeaderIdentifierOfAPC::GRAPH_MUTATION_AND_LOCK);
        
        if (
            !ReadASnapShotFromSlab(
            identity_begin,
            APCDataStructure::TotalIdentityUnitCount(),
            identity.data(),
            true
            )
        )
        {
            return std::nullopt;
        }
        
        return current_state;
    }


    // std::optional<uint64_t> ConstructAPC::SwitchIdentityState__(
    //     IAB::GraphMutationState desired_state,
    //     uint32_t apc_slot,
    //     std::optional<IAB::GraphMutationState> required_state,
    //     uint32_t max_tries
    // ) noexcept
    // {
    //     DSA::InternalAPCRange range_of_apc{};

    //     std::optional<DescriptionOfAPC::StateOfAPC> dsc_state = ReadValidAPCRangeInternally__(
    //         apc_slot,
    //         range_of_apc
    //     );

    //     if (
    //         !dsc_state.has_value() ||
    //         dsc_state != DSA::StateOfAPC::LIVE ||
    //         !range_of_apc.IsValid
    //     )
    //     {
    //         return false;
    //     }
    //     const size_t fp_lock_idx = range_of_apc.BeginIndex + static_cast<uint8_t>(HeaderIdentifierOfAPC::GRAPH_MUTATION_AND_LOCK);
    //     uint64_t value_fp_lock = FABRIC_CELL_SENTINAL;
    //     if (
    //         !AtomicallyLoadReadAUnit(fp_lock_idx, value_fp_lock) ||
    //         !APCDataStructure::IsValidFabricUnit(value_fp_lock)
    //     )
    //     {
    //         return false;
    //     }
    //     const IAB::GraphMutationState fp_state = IAB::IdentityFingerprintToState(value_fp_lock);


        
        


    // }



}