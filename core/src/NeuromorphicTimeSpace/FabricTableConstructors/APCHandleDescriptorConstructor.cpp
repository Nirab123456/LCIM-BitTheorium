#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace PredictedAdaptedEncoding
{

    APCSegmentPoolRange APCHandleDescriptorConstructor::GetSegmentPoolBegainEndForSingleAPCDescription(uint64_t single_description_index) noexcept
    {
        
        APCSegmentPoolRange desired_segment_pool_range{};

        if (
            single_description_index >= CountOfAPC_ ||
            PerAPCRuntimeCellCount_ == UNSIGNED_ZERO
        )
        {
            return desired_segment_pool_range;
        }


        const uint64_t apc_count_offset = single_description_index * PerAPCRuntimeCellCount_;
        desired_segment_pool_range.BeginIndex = SegmentPoolBegin_ + static_cast<size_t>(apc_count_offset);
        desired_segment_pool_range.EndIndex = desired_segment_pool_range.BeginIndex + static_cast<size_t>(PerAPCRuntimeCellCount_);
        if (
            desired_segment_pool_range.BeginIndex >= SegmentPoolBegin_ &&
            desired_segment_pool_range.BeginIndex < desired_segment_pool_range.EndIndex &&
            desired_segment_pool_range.EndIndex <= SegmentPoolEnd_ &&
            desired_segment_pool_range.EndIndex <= SlabCellCount_
        )
        {
            desired_segment_pool_range.IsValid = true;
        }

        return desired_segment_pool_range;
        
    }

    APCDescriptorRange APCHandleDescriptorConstructor::ReadARangeOfAPCDescription_(uint64_t apc_slot_index) noexcept
    {
        APCDescriptorRange probable_full_range_of_apc_descriptor{};
        const bool ok = ReadAPCDescriptorTableBeginEndFromRecordBook(probable_full_range_of_apc_descriptor);

        APCDescriptorRange desired_slot_of_apc_descriptor{};

        if (!ok)
        {
            return desired_slot_of_apc_descriptor;
        }

        desired_slot_of_apc_descriptor.BeginIndex = probable_full_range_of_apc_descriptor.BeginIndex + static_cast<size_t>(apc_slot_index) * APC_DESCRIPTOR_WIDTH_OR_VALIDATION_INDEX;
        desired_slot_of_apc_descriptor.EndIndex = desired_slot_of_apc_descriptor.BeginIndex + APC_DESCRIPTOR_WIDTH_OR_VALIDATION_INDEX;
        desired_slot_of_apc_descriptor.IsValid = true;
        return desired_slot_of_apc_descriptor;
    }


    bool APCHandleDescriptorConstructor::ReadACompleateAPCDescriptorBuffer(
        uint64_t apc_description_index, 
        DescriptionOfAPC::SingleAPCDescriptionCellBuffer& return_buffer,
        bool claimed_is_invalid,
        OwnershipPolicy validate_observer,
        DescriptionOfAPC::StateOfSingleAPCDescription desired_state,
        std::optional<uint8_t> version_match
    ) noexcept
    {
        if (apc_description_index >= CountOfAPC_)
        {
            return false;
        }

        DescriptionOfAPC::BuildABlankAPCDescriptionBufferwith2CellIdentity(return_buffer);

        const APCDescriptorRange this_apc_descriptor_range = ReadARangeOfAPCDescription_(apc_description_index);

        if (!this_apc_descriptor_range.IsValid)
        {
            return false;
        }

        std::memcpy(
            return_buffer.data(),
            &SlabBasePtr_[this_apc_descriptor_range.BeginIndex],
            APC_DESCRIPTOR_WIDTH_OR_VALIDATION_INDEX * sizeof(packed64_t)
        );

        return DescriptionOfAPC::ValidateSingleAPCDescriptionBuffer(
            return_buffer,
            claimed_is_invalid,
            validate_observer,
            desired_state,
            apc_description_index,
            version_match
        );
    }


    bool APCHandleDescriptorConstructor::ClaimACompleateAPCDescriptorCells(uint64_t apc_description_idx) noexcept
    {
        if (apc_description_idx >= CountOfAPC_)
        {
            return false;
        }

        const APCDescriptorRange this_apc_descriptor_range = ReadARangeOfAPCDescription_(apc_description_idx);
        if (!this_apc_descriptor_range.IsValid)
        {
            return false;
        }
        
        return ClaimNxSequentialPackedCellStrong(this_apc_descriptor_range.BeginIndex, APC_DESCRIPTOR_WIDTH_OR_VALIDATION_INDEX);
    }



    bool APCHandleDescriptorConstructor::OneShotUpdateAPCDescriptor(
        DescriptionOfAPC::SingleAPCDescriptionCellBuffer& a_valid_description_buffer,
        bool caller_holds_claim_guard
    ) noexcept
    {
        std::optional<uint64_t> current_descriptor_idx = DescriptionOfAPC::ReadADataValueFromADescriptionBuffer(a_valid_description_buffer, APCDescriptorCellType::CURRENT_DESCRIPTOR_INDEX);
        if (!current_descriptor_idx.has_value())
        {
            return false;
        }

        const APCDescriptorRange desired_descriptor_range = ReadARangeOfAPCDescription_(current_descriptor_idx.value());
        if (!desired_descriptor_range.IsValid)
        {
            return false;
        }

        if (!caller_holds_claim_guard)
        {
            return ClaimThenMemCopyFromArray_(
                desired_descriptor_range.BeginIndex,
                APC_DESCRIPTOR_WIDTH_OR_VALIDATION_INDEX,
                a_valid_description_buffer
            );
        }
        else
        {
            return ForceMemCopyFromArray_(
                desired_descriptor_range.BeginIndex,
                APC_DESCRIPTOR_WIDTH_OR_VALIDATION_INDEX,
                a_valid_description_buffer
            );
        }
    }
    
    DescriptionOfAPC::DescriptorSaftyFiles APCHandleDescriptorConstructor::OneShotTryReadingDescriptionState_(uint64_t apc_description_index) noexcept
    {
        DescriptionOfAPC::DescriptorSaftyFiles return_files{};

        if (!SlabBasePtr_ || apc_description_index >= CountOfAPC_)
        {
            return return_files;
        }
        
        const APCDescriptorRange desired_description_range = ReadARangeOfAPCDescription_(apc_description_index);
        if (!desired_description_range.IsValid)
        {
            return return_files;
        }
        const size_t state_cell_idx = desired_description_range.BeginIndex + static_cast<size_t>(APCDescriptorCellType::STATE_OWNERSHIP_VESION_SAFTY);
        const packed64_t state_of_apc_cell = SlabBasePtr_[state_cell_idx];

        return_files = DescriptionOfAPC::ReadFilesFromStateSaftyofADescriptor(state_of_apc_cell);
        return return_files;
    }


    bool APCHandleDescriptorConstructor::SwitchOwnershipOfAReadyDescription(
        uint64_t description_idx,
        OwnershipPolicy updated_owner,
        DescriptionOfAPC::StateOfSingleAPCDescription updated_state
    ) noexcept
    {
        DescriptionOfAPC::SingleAPCDescriptionCellBuffer  desired_apc_description_buffer{};
        
        const bool buffer_ok = ReadACompleateAPCDescriptorBuffer(
            description_idx, 
            desired_apc_description_buffer, 
            false, 
            OwnershipPolicy::NEUROMORPHIC_SPACE_TIME_FABRIC
        );
        if (!buffer_ok)
        {
            return false;
        }

        const packed64_t updated_safty = DescriptionOfAPC::SwitchStateOrAPCOwnerOfSaftyCell(
            desired_apc_description_buffer[static_cast<size_t>(APCDescriptorCellType::STATE_OWNERSHIP_VESION_SAFTY)],
            updated_state,
            updated_owner
        );

        const bool update_ok_safty = DescriptionOfAPC::SetStateSaftyCellInBuffer(desired_apc_description_buffer, updated_safty);
        if (!update_ok_safty)
        {
            return false;
        }

        const bool claimed_descripor_for_caller = OneShotUpdateAPCDescriptor(desired_apc_description_buffer, true);
        if (!claimed_descripor_for_caller)
        {
            return false;
        }

        return true;
    }


    std::optional<uint64_t> APCHandleDescriptorConstructor::GetASlotForNewAPCLink() noexcept
    {
        if (
            !FabricInitialized_.load(MoLoad_) ||
            !SlabBasePtr_ || 
            APCDataStructure::IsCapacityOfAPCValid(PerAPCRuntimeCellCount_) ||
            !HashIdConstructror::IsValidAPCId48(CountOfAPC_)
        )
        {
            return std::nullopt;
        }

        uint64_t desired_apc_slot = PackedCell64_t::PACKED_CELL_SENTINAL;

        for (uint64_t description_idx = 0; description_idx < CountOfAPC_; description_idx++)
        {
            const DescriptionOfAPC::DescriptorSaftyFiles desired_files = OneShotTryReadingDescriptionState_(description_idx);
            if (
                desired_files.IsValid && 
                desired_files.WidthOfAPC == PerAPCRuntimeCellCount_ &&
                desired_files.LocalityOfTheDescription ==LocalityPolicy::PUBLISHED && 
                desired_files.WhoHoldsTheAcess != OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER &&  
                desired_files.StateOfTheAPC == DescriptionOfAPC::StateOfSingleAPCDescription::RECORD_WITH_SEGMENT_POOL &&
                ClaimACompleateAPCDescriptorCells(description_idx)
            )
            {
                desired_apc_slot = description_idx;
                break;
            }
        }

        if (desired_apc_slot >= CountOfAPC_)
        {
            return std::nullopt;
        }

        bool switch_ok = SwitchOwnershipOfAReadyDescription(
            desired_apc_slot, 
            OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER, 
            DescriptionOfAPC::StateOfSingleAPCDescription::OWNED_BY_APC
        );

        if (!switch_ok)
        {
            return std::nullopt;
        }
        
        return desired_apc_slot;
    }


}
