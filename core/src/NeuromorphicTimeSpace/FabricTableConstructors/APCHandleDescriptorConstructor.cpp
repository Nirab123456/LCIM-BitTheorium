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

    DescriptorConf::APCDescriptorRange APCHandleDescriptorConstructor::ReadAPCDescriptionRanges(uint64_t apc_slot_index) noexcept
    {
        RecordBookConf::RecordBookTablesBoundsCarrier descripor_directory_map{};
        const bool ok = GetRecordMapCarrierRanges(
            FabricTableSegmentClasses::APC_HANDLE_DESCRIPTOR,
            descripor_directory_map
        );

        DescriptorConf::APCDescriptorRange desired_slot_of_apc_descriptor{};
        if (
            !ok ||
            apc_slot_index >= CountOfAPC_ ||
            descripor_directory_map.EndIndex > SlabCellCount_
        )
        {
            desired_slot_of_apc_descriptor.IsValid = false;
            return desired_slot_of_apc_descriptor;
        }

        desired_slot_of_apc_descriptor.BeginIndex = descripor_directory_map.BeginIndex + static_cast<size_t>(apc_slot_index) * DescriptionOfAPC::DESCRIPTION_WIDTH_AND_VALIDATION_IDX;
        desired_slot_of_apc_descriptor.EndIndex = desired_slot_of_apc_descriptor.BeginIndex + DescriptionOfAPC::DESCRIPTION_WIDTH_AND_VALIDATION_IDX;
        desired_slot_of_apc_descriptor.IsValid = 
            desired_slot_of_apc_descriptor.BeginIndex >= descripor_directory_map.BeginIndex &&
            desired_slot_of_apc_descriptor.BeginIndex < desired_slot_of_apc_descriptor.EndIndex &&
            desired_slot_of_apc_descriptor.EndIndex <= descripor_directory_map.EndIndex;
        return desired_slot_of_apc_descriptor;
    }


    bool APCHandleDescriptorConstructor::ReadACompleateAPCDescriptorBuffer(
        uint64_t apc_description_index, 
        DescriptionOfAPC::SingleAPCDescriptionCellBuffer& return_buffer
    ) noexcept
    {
        if (apc_description_index >= CountOfAPC_)
        {
            return false;
        }

        DescriptionOfAPC::BuildSentinalDescriptionBuffer(return_buffer);

        const DescriptorConf::APCDescriptorRange this_apc_descriptor_range = ReadAPCDescriptionRanges(apc_description_index);

        if (!this_apc_descriptor_range.IsValid)
        {
            return false;
        }
        if (
            !ReadASnapShotFromSlab(
                this_apc_descriptor_range.BeginIndex,
                DescriptionBuffer::DESCRIPTION_WIDTH_AND_VALIDATION_IDX,
                return_buffer.data(),
                true
            )
        )
        {
            return false;
        }
        return DescriptionOfAPC::ValidateADescriptionBuffer(return_buffer);
    }



    bool APCHandleDescriptorConstructor::OneShotUpdateAPCDescriptor(
        const DescriptionOfAPC::SingleAPCDescriptionCellBuffer& desc_buffer
    ) noexcept
    {
        const DescriptionOfAPC::APCDescriptorRange desired_descriptor_range = ReadAPCDescriptionRanges(
            desc_buffer[static_cast<size_t>(DescriptionOfAPC::DescriptionUnitIdentity::APC_INDEX)]
        );

        if (
            !desired_descriptor_range.IsValid ||
            !DescriptionOfAPC::HasValidationDescriptionMark(desc_buffer)
        )
        {
            return false;
        }

        return AtomicallyCopyFromBufferToFabric(
            desired_descriptor_range.BeginIndex,
            DescriptionOfAPC::DESCRIPTION_WIDTH_AND_VALIDATION_IDX,
            desc_buffer.data()
        );
    }
    
    DescriptionOfAPC::DescriptorSaftyFiles APCHandleDescriptorConstructor::ReadAPCStateAtomically(uint64_t apc_description_index) noexcept
    {
        DescriptionOfAPC::DescriptorSaftyFiles return_files{};
        std::optional<size_t> maybe_id_state_idx = GetIdStateIdxByDescriptionIdx(apc_description_index);
        if (!maybe_id_state_idx.has_value())
        {
            return return_files;
        }
        uint64_t state_of_apc_cell = UNSIGNED_ZERO;
        AtomicallyLoadReadAUnit(maybe_id_state_idx.value(), state_of_apc_cell);
        return_files = DescriptionOfAPC::GetDescriptionFile(state_of_apc_cell);
        return return_files;
    }


    bool APCHandleDescriptorConstructor::SwitchOwnershipOfAReadyDescription(
        uint64_t description_idx,
        DescriptionOfAPC::StateOfAPC desired_state
    ) noexcept
    {
        DescriptionOfAPC::SingleAPCDescriptionCellBuffer  desc_buffer{};
        
        const bool buffer_ok = ReadACompleateAPCDescriptorBuffer(
            description_idx, 
            desc_buffer
        );
        const uint32_t updated_id = DescriptionOfAPC::ComposeDescriptionId(
            desc_buffer, desired_state
        );
        uint64_t updated_id_state = DescriptionOfAPC::ComposeIdAndState(updated_id, desired_state);
        std::optional<size_t> maybe_id_state_idx = GetIdStateIdxByDescriptionIdx(description_idx);

        uint64_t expected_id_state = desc_buffer[
            static_cast<size_t>(DescriptionOfAPC::DescriptionUnitIdentity::ID_STATE_CONCURRENT)
        ];

        const DescriptionOfAPC::DescriptorSaftyFiles expected_desc_files = DescriptionOfAPC::GetDescriptionFile(expected_id_state);

        if (
            !buffer_ok ||
            !APCDataStructure::IsValid32BitAPCUnit(updated_id) ||
            !APCDataStructure::IsValidFabricUnit(updated_id_state) ||
            !maybe_id_state_idx.has_value() ||
            !expected_desc_files.IsValid ||
            !DescriptionOfAPC::IsTransitionStateLeagal(expected_desc_files.StateOfTheAPC, desired_state)
        )
        {
            return false;
        }

        return CompareExchangeStrongFromFabric(
            maybe_id_state_idx.value(),
            expected_id_state,
            updated_id_state
        );
    }


    std::optional<uint64_t> APCHandleDescriptorConstructor::GetASlotForNewAPCLink() noexcept
    {
        if (
            !FabricInitialized_.load(std::memory_order_acquire) ||
            !SlabBasePtr_ || 
            !APCDataStructure::IsCapacityOfAPCValid(PerAPCRuntimeCellCount_) ||
            !HashIdConstructror::IsValidAPCId(CountOfAPC_)
        )
        {
            return std::nullopt;
        }

        for (uint64_t description_idx = 0; description_idx < CountOfAPC_; description_idx++)
        {
            const DescriptionOfAPC::DescriptorSaftyFiles desired_files = ReadAPCStateAtomically(description_idx);
            if (
                desired_files.IsValid && 
                desired_files.StateOfTheAPC == DescriptionOfAPC::StateOfAPC::FREE_OR_EMPTY &&
                SwitchOwnershipOfAReadyDescription(
                    description_idx,
                    DescriptionOfAPC::StateOfAPC::RESERVED
                )
            )
            {
                return static_cast<uint64_t>(description_idx);
            }
        }

        return std::nullopt;
    }


    std::optional<size_t> APCHandleDescriptorConstructor::GetIdStateIdxByDescriptionIdx(uint64_t description_idx) noexcept
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



}
