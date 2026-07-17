#pragma once 
#include "RecordBookConstructor.h"

namespace PredictedAdaptedEncoding
{
    class APCHandleDescriptorConstructor : public RecordBookConstructor
    {
    protected:

        DescriptorConf::APCDescriptorRange ReadAPCDescriptionOnSlotIdx(uint64_t apc_slot_index) noexcept;

    public:

        APCSegmentPoolRange GetSegmentPoolBegainEndForSingleAPCDescription(uint64_t single_description_index) noexcept;

        bool ReadACompleateAPCDescriptorBuffer(
            uint64_t apc_description_index, 
            DescriptionOfAPC::SingleAPCDescriptionCellBuffer& return_buffer,
            DescriptorConf::StateOfAPC desired_state = DescriptorConf::StateOfAPC::UNASSIGNED_UNUSED_NANNULL,
            std::optional<uint8_t> version_match = std::nullopt
        ) noexcept;

        /// @brief UPDATES: A Description In ONE SHOT
        bool OneShotUpdateAPCDescriptor(
            DescriptionOfAPC::SingleAPCDescriptionCellBuffer& a_valid_description_buffer,
            bool caller_holds_claim_guard = false
        ) noexcept;

        /// @brief Just Reads the APCDescriptorCellType::ID_STATE_CONCURRENT Cell  Without Validating with Other Descriptor Cells
        /// @param apc_description_index 
        /// @return 
        DescriptionOfAPC::DescriptorSaftyFiles OneShotTryReadingDescriptionState_(uint64_t apc_description_index) noexcept;


        bool SwitchOwnershipOfAReadyDescription(
            uint64_t description_idx,
            OwnershipPolicy updated_owner,
            DescriptionOfAPC::StateOfSingleAPCDescription updated_state
        ) noexcept;

        std::optional<uint64_t> GetASlotForNewAPCLink() noexcept;
        
    };

}