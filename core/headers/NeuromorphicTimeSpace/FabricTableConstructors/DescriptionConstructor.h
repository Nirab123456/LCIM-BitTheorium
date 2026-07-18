#pragma once 
#include "RecordBookConstructor.h"

namespace PredictedAdaptedEncoding
{
    class APCHandleDescriptorConstructor : public RecordBookConstructor
    {
    protected:

        DescriptorConf::APCDescriptorRange ReadAPCDescriptionRanges(uint64_t apc_slot_index) noexcept;
    
        std::optional<size_t> GetIdStateIdxOnDescriptionIdx(uint64_t description_idx) noexcept
        {
            const DescriptionOfAPC::APCDescriptorRange desired_description_range = ReadAPCDescriptionRanges(description_idx);
            if (!desired_description_range.IsValid)
            {
                return std::nullopt;
            }

            const size_t state_cell_idx = desired_description_range.BeginIndex + 
                static_cast<size_t>(DescriptionOfAPC::DescriptionUnitIdentity::ID_STATE_CONCURRENT);
            
            return state_cell_idx;
        }


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
            const DescriptionOfAPC::SingleAPCDescriptionCellBuffer& a_valid_description_buffer
        ) noexcept;

        /// @brief Just Reads the DescriptionUnitIdentity::ID_STATE_CONCURRENT Cell  Without Validating with Other Descriptor Cells
        /// @param apc_description_index 
        /// @return 
        DescriptionOfAPC::DescriptorSaftyFiles ReadAPCStateAtomically(uint64_t apc_description_index) noexcept;


        bool SwitchOwnershipOfAReadyDescription(
            uint64_t description_idx,
            DescriptionOfAPC::StateOfAPC updated_state
        ) noexcept;

        std::optional<uint64_t> GetASlotForNewAPCLink() noexcept;



        
    };

}