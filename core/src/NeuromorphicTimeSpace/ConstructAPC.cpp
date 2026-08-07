#pragma once
#include "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"
#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace BidirectionalInMemGraph
{

    std::optional<DescriptionOfAPC::StateOfAPC> ConstructAPC::ReadIdentityBufferOfAPC(
        uint32_t apc_slot,
        IAB::BufferOfAPCIdentity& identity
    ) noexcept
    {
        DSA::SingleAPCDescriptionCellBuffer desc_buffer{};
        IAB::BuildNullIdentityBuffer(identity);
        if (
            !ReadACompleateAPCDescriptorBuffer_(apc_slot, desc_buffer) ||
            !DSA::ValidateADescriptionBuffer(desc_buffer)
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


        const size_t identity_begin = desc_buffer[static_cast<uint8_t>(DSA::DescriptionIndexing::APC_SEGMENTPOOL_BEGAIN_SLAB)] +
            static_cast<uint8_t>(HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT);
        
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
        
        return state.StateOfTheAPC;

    }


}