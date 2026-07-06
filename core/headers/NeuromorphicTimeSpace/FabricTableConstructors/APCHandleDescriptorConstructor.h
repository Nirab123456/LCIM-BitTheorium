#pragma once 
#include "RecordBookConstructor.h"

namespace PredictedAdaptedEncoding
{
    class APCHandleDescriptorConstructor : public RecordBookConstructor
    {
    protected:
        /// @brief Read Description OF:: Single APC SLOT ||(WARNING: IT DOSENT READ THE MEMORY RANGE OF THE APC)
        /// @param apc_slot_index 
        /// @return 
        APCDescriptorRange ReadARangeOfAPCDescription_(uint64_t apc_slot_index) noexcept;

    public:

        /// @brief Directly Gets Segment Pool Range For An APC by using INDEX: Of FabricTableSegmentClasses::APC_HANDLE_DESCRIPTOR | Dosent validate handle OR: Initialization 
        /// @param single_description_index Sequential index of desired APC FabricTableSegmentClasses::APC_HANDLE_DESCRIPTOR
        /// @return VALID: APCDescriptorRange::IsValid -> true || INVALID: APCDescriptorRange::IsValid -> true-> false
        APCSegmentPoolRange GetSegmentPoolBegainEndForSingleAPCDescription(uint64_t single_description_index) noexcept;

        /// @brief Returns A return_buffer bbased obn set rules
        /// @param claimed_is_invalid MEANS: IF true -> LocalityPolicy::CLAIMED ->INVALID
        /// @param validate_observer Fabric can observe when ownership to APC but APC cant observe when ownership to fabric 
        /// @param desired_state  APC shouldnt observe StateOfSingleAPCDescription::EMPTY_RECORD / StateOfSingleAPCDescription::LOGICALY_RETIRED
        /// @param version_match VERSION:: Probably optional for now 
        /// @return 
        bool ReadACompleateAPCDescriptorBuffer(
            uint64_t apc_description_index, 
            DescriptionOfAPC::SingleAPCDescriptionCellBuffer& return_buffer,
            bool claimed_is_invalid = true,
            OwnershipPolicy validate_observer = OwnershipPolicy::UNASSIGNED_UNUSED_NANNULL,
            DescriptionOfAPC::StateOfSingleAPCDescription desired_state = DescriptionOfAPC::StateOfSingleAPCDescription::UNASSIGNED_UNUSED_NANNULL,
            std::optional<uint8_t> version_match = std::nullopt
        ) noexcept;

        /// @brief CLAIMS: LocalityPolicy::CLAIMED Set TO: ALL: OF the descriptor Cells 
        /// @param apc_description_idx INDEX of the DEscription
        /// @return If Claimed -> true / false
        bool ClaimACompleateAPCDescriptorCells(uint64_t apc_description_idx) noexcept;

        /// @brief UPDATES: A Description In ONE SHOT
        bool OneShotUpdateAPCDescriptor(
            DescriptionOfAPC::SingleAPCDescriptionCellBuffer& a_valid_description_buffer,
            bool caller_holds_claim_guard = false
        ) noexcept;

        /// @brief Just Reads the APCDescriptorCellType::STATE_OWNERSHIP_VESION_SAFTY Cell  Without Validating with Other Descriptor Cells
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