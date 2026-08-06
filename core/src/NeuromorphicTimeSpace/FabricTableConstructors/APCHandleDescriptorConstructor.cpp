#include "NeuromorphicTimeSpace/SlabToFabricConverterAndCordinator.h"

namespace BidirectionalInMemGraph
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

    DescriptorConf::APCDescriptorRange APCHandleDescriptorConstructor::ReadAPCDescriptionRanges_(uint64_t apc_slot_index) noexcept
    {
        RecordBookConf::RecordBookTablesBoundsCarrier descripor_directory_map{};
        const bool ok = GetRecordMapCarrierRanges_(
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


    bool APCHandleDescriptorConstructor::ReadACompleateAPCDescriptorBuffer_(
        uint64_t apc_description_index, 
        DescriptionOfAPC::SingleAPCDescriptionCellBuffer& return_buffer
    ) noexcept
    {
        if (apc_description_index >= CountOfAPC_)
        {
            return false;
        }

        DescriptionOfAPC::BuildSentinalDescriptionBuffer(return_buffer);

        const DescriptorConf::APCDescriptorRange this_apc_descriptor_range = ReadAPCDescriptionRanges_(apc_description_index);

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
        return true;
    }



    bool APCHandleDescriptorConstructor::OneShotUpdateReservedDescription_(
        DescriptionOfAPC::SingleAPCDescriptionCellBuffer& desc_buffer
    ) noexcept
    {
        const uint64_t slot_idx = desc_buffer[static_cast<size_t>(DescriptionOfAPC::DescriptionIdentity::APC_INDEX)];
        const DescriptionOfAPC::APCDescriptorRange desired_descriptor_range = ReadAPCDescriptionRanges_(slot_idx);
        const  DescriptionOfAPC::DescriptorSaftyFiles current_description_state = ReadAPCStateAtomically_(slot_idx);
        if (
            current_description_state.StateOfTheAPC != DSA::StateOfAPC::RESERVED ||
            !desired_descriptor_range.IsValid ||
            !DescriptionOfAPC::ValidateADescriptionBuffer(desc_buffer)
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
    
    DescriptionOfAPC::DescriptorSaftyFiles APCHandleDescriptorConstructor::ReadAPCStateAtomically_(uint64_t apc_description_index) noexcept
    {
        DescriptionOfAPC::DescriptorSaftyFiles return_files{};
        std::optional<size_t> maybe_id_state_idx = GetIdStateIdxByDescriptionIdx_(apc_description_index);
        if (!maybe_id_state_idx.has_value())
        {
            return return_files;
        }
        uint64_t state_of_apc_cell = FABRIC_CELL_SENTINAL;
        AtomicallyLoadReadAUnit(maybe_id_state_idx.value(), state_of_apc_cell);
        return_files = DescriptionOfAPC::GetDescriptionFile(state_of_apc_cell);
        return return_files;
    }


    std::optional<uint64_t> APCHandleDescriptorConstructor::SwitchOwnershipOfAReadyDescription(
        uint64_t description_idx,
        DSA::StateOfAPC desired_state,
        bool caller_holds_reservation,
        uint32_t max_tries
    ) noexcept
    {
        const DSA::APCDescriptorRange this_apc_descriptor_range = ReadAPCDescriptionRanges_(description_idx);

        if (!this_apc_descriptor_range.IsValid)
        {
            return std::nullopt;
        }
        const uint8_t id_st_concurrent = static_cast<uint8_t>(DSA::DescriptionIdentity::ID_STATE_CONCURRENT);
        const uint64_t id_state_idx = this_apc_descriptor_range.BeginIndex + id_st_concurrent;

        DSA::SingleAPCDescriptionCellBuffer description_buffer{};

        for (size_t i = 0; i < max_tries; i++)
        {
            const DSA::DescriptorSaftyFiles current_id_st = ReadAPCStateAtomically_(description_idx);
            if (
                !current_id_st.IsValid
            )
            {
                return std::nullopt;
            }

            const bool false_owner_claim = caller_holds_reservation && current_id_st.StateOfTheAPC != DSA::StateOfAPC::RESERVED;
            const bool non_owner_touching_reserved = !caller_holds_reservation && current_id_st.StateOfTheAPC == DSA::StateOfAPC::RESERVED;
            if (
                false_owner_claim || 
                non_owner_touching_reserved ||
                !DSA::IsTransitionStateLeagal(current_id_st.StateOfTheAPC, desired_state)
            )
            {
                continue;
            }
            uint64_t current_id_state_value = DSA::ComposeIdAndState(current_id_st.DescriptionID, current_id_st.StateOfTheAPC);

            if (
                !ReadACompleateAPCDescriptorBuffer_(description_idx, description_buffer) ||
                description_buffer[id_st_concurrent] != current_id_state_value
            )
            {
                continue;
            }

            DSA::DescriptorSaftyFiles updated_id_state{};
            updated_id_state.StateOfTheAPC = desired_state;
            updated_id_state.DescriptionID = DSA::ComposeDescriptionId(
                description_buffer,
                updated_id_state.StateOfTheAPC
            );

            const uint64_t updated_id_state_value = DSA::ComposeIdAndState(updated_id_state.DescriptionID, updated_id_state.StateOfTheAPC);

            if (
                APCDataStructure::IsValidFabricUnit(updated_id_state_value) &&
                CompareExchangeStrongFromFabric(
                    id_state_idx,
                    current_id_state_value,
                    updated_id_state_value
                )
            )
            {
                return current_id_state_value;
            }
            
        }
        return std::nullopt;
    }


    std::optional<uint64_t> APCHandleDescriptorConstructor::GetASlotForNewAPCLink(uint64_t& desired_slot) noexcept
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
            const DSA::DescriptorSaftyFiles current = ReadAPCStateAtomically_(description_idx);
            if (
                !current.IsValid ||
                (
                    current.StateOfTheAPC != DSA::StateOfAPC::FREE_OR_EMPTY &&
                    current.StateOfTheAPC != DSA::StateOfAPC::RETIRED_OR_TOMBSTONE
                )
            )
            {
                continue;
            }
            
            std::optional<uint64_t> previous_st_id_value = SwitchOwnershipOfAReadyDescription(
                description_idx,
                DescriptionOfAPC::StateOfAPC::RESERVED,
                false
            );
            
            if (!previous_st_id_value.has_value())
            {
                continue;
            }
            desired_slot = description_idx;
            return previous_st_id_value;
        }

        return std::nullopt;
    }


    std::optional<size_t> APCHandleDescriptorConstructor::GetIdStateIdxByDescriptionIdx_(uint64_t description_idx) noexcept
    {
        const DescriptionOfAPC::APCDescriptorRange desired_description_range = ReadAPCDescriptionRanges_(description_idx);
        if (!desired_description_range.IsValid)
        {
            return std::nullopt;
        }

        const size_t state_cell_idx = desired_description_range.BeginIndex + 
            static_cast<size_t>(DescriptionOfAPC::DescriptionIdentity::ID_STATE_CONCURRENT);
        
        return state_cell_idx;
    }



}
