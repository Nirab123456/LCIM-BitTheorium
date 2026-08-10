#pragma once 
#include "FabricTableOrchestrator.hpp"

namespace BidirectionalInMemGraph
{

    struct DescriptorConf : public CoreOfFabricCoordinator
    {

        struct APCDescriptorRange
        {
            size_t BeginIndex = UNSIGNED_ZERO;
            size_t EndIndex = UNSIGNED_ZERO;
            bool IsValid = false;
        };

        using InternalAPCRange = APCDescriptorRange;

        enum class StateOfAPC : uint8_t
        {
            FREE = 0,
            RESERVED = 1,
            LIVE = 2,
            RETIRED = 3,
            HAULTED = 4
        };
        
        struct DescriptionStateLockValues
        {
            uint32_t SeqLock = UINT32_MAX;
            StateOfAPC StateOfTheAPC = StateOfAPC::RETIRED;
            bool IsValid = false;
        };

        static_assert(sizeof(DescriptionStateLockValues) <= sizeof(uint64_t));

        static constexpr uint64_t ComposeIdAndState(DescriptionStateLockValues& files) noexcept
        {
            if (
                !APCDataStructure::IsValid32BitAPCUnit(files.SeqLock) ||
                !ValidateDescriptionFiles(files)
            )
            {
                return FABRIC_CELL_SENTINAL;
            }
            return TwinU32ToU64::PackDoubleUnsigned32In64(files.SeqLock, static_cast<uint32_t>(files.StateOfTheAPC));
        }

        static constexpr DescriptionStateLockValues GetDescriptionFile(uint64_t desc_id_state) noexcept
        {
            DescriptionStateLockValues return_safty_files{};
            return_safty_files.SeqLock = TwinU32ToU64::ExtractLow32Of64(desc_id_state);
            return_safty_files.StateOfTheAPC = static_cast<StateOfAPC>(TwinU32ToU64::ExtractHigh32Of64(desc_id_state));

            if (!APCDataStructure::IsValidFabricUnit(return_safty_files.SeqLock))
            {
                return return_safty_files;
            }
            ValidateDescriptionFiles(return_safty_files);
            return return_safty_files;
        }

        static constexpr bool ValidateDescriptionFiles(DescriptionStateLockValues& files) noexcept
        {
            if (!APCDataStructure::IsValidFabricUnit(files.SeqLock))
            {
                files.IsValid = false;
                return false;
            }
            if (
                files.StateOfTheAPC == StateOfAPC::RESERVED &&
                InstallAxisToBuffer::IsValidEven64(files.SeqLock)
            )
            {
                files.IsValid = false;
                return false;
            }
            if (
                files.StateOfTheAPC != StateOfAPC::RESERVED &&
                !InstallAxisToBuffer::IsValidEven64(files.SeqLock)
            )
            {
                files.IsValid = false;
                return false;
            }

            files.IsValid = true;
            return true;
        }

        static constexpr bool IsTransitionStateLeagal(StateOfAPC current_state, StateOfAPC desired_state) noexcept
        {
            return (current_state == StateOfAPC::FREE && desired_state == StateOfAPC::RESERVED) ||
                (current_state == StateOfAPC::RESERVED && desired_state == StateOfAPC::FREE) ||
                (current_state == StateOfAPC::RESERVED && desired_state == StateOfAPC::LIVE) ||
                (current_state == StateOfAPC::LIVE && desired_state == StateOfAPC::RESERVED) ||
                (current_state == StateOfAPC::RESERVED && desired_state == StateOfAPC::RETIRED) ||
                (current_state == StateOfAPC::RETIRED && desired_state == StateOfAPC::RESERVED) ||
                (current_state == StateOfAPC::LIVE && desired_state == StateOfAPC::HAULTED) ||
                (current_state == StateOfAPC::HAULTED && desired_state == StateOfAPC::LIVE);
                
        }

    };


    struct DescriptionOfAPC : DescriptorConf
    {

        using SingleAPCDescriptionCellBuffer = std::array<uint64_t, DESCRIPTION_WIDTH_AND_VALIDATION_IDX>;

        static constexpr bool ValidateADescriptionBuffer(
            SingleAPCDescriptionCellBuffer& desc_return_buff
        ) noexcept
        {
            for (size_t i = 0; i < DESCRIPTION_WIDTH_AND_VALIDATION_IDX; i++)
            {
                if (!APCDataStructure::IsValidFabricUnit(desc_return_buff[i]))
                {
                    return false;
                }
            }

            const uint64_t span_of_apc = desc_return_buff[static_cast<size_t>(DescriptionIndexing::APC_SEGMENTPOOL_END_SLAB)] -
                desc_return_buff[static_cast<size_t>(DescriptionIndexing::APC_SEGMENTPOOL_BEGAIN_SLAB)];
            
            const DescriptionStateLockValues desc_files = GetDescriptionFile(
                desc_return_buff[static_cast<size_t>(DescriptionIndexing::ID_STATE_CONCURRENT)]
            );

            return 
                APCDataStructure::IsCapacityOfAPCValid(static_cast<uint32_t>(span_of_apc)) &&
                desc_files.IsValid;
        }

        static constexpr void SetADescriptionUnit(
            SingleAPCDescriptionCellBuffer& desc_buffer,
            DescriptionIndexing identity,
            uint64_t value
        ) noexcept
        {
            if (
                !APCDataStructure::IsValidFabricUnit(value)
            )
            {
                return;
            }
            desc_buffer[static_cast<size_t>(identity)] = value;
        }

        static constexpr bool ConstructInitialAPCDescriptionBuffer(
            SingleAPCDescriptionCellBuffer& desc_return_buff,
            uint64_t apc_idx,
            uint64_t segment_pool_begin,
            uint64_t segment_pool_end
        ) noexcept
        {
            desc_return_buff.fill(UNSIGNED_ZERO);
            SetADescriptionUnit(desc_return_buff, DescriptionIndexing::APC_INDEX, apc_idx);
            SetADescriptionUnit(desc_return_buff, DescriptionIndexing::APC_SEGMENTPOOL_BEGAIN_SLAB, segment_pool_begin);
            SetADescriptionUnit(desc_return_buff, DescriptionIndexing::APC_SEGMENTPOOL_END_SLAB, segment_pool_end);

            DescriptionStateLockValues files{};
            files.SeqLock = 2u;
            files.StateOfTheAPC = StateOfAPC::FREE;

            const uint64_t id_state_unit = ComposeIdAndState(files);
            if (
                !APCDataStructure::IsValidFabricUnit(id_state_unit)
            )
            {
                return false;
            }
            
            SetADescriptionUnit(desc_return_buff, DescriptionIndexing::ID_STATE_CONCURRENT, id_state_unit);
            return ValidateADescriptionBuffer(desc_return_buff);
        }

        static constexpr bool BuildIdentityBufferFromDescriptionBuffer(
            SingleAPCDescriptionCellBuffer& description,
            InstallAxisToBuffer::BufferOfAPCIdentity& identity
        ) noexcept
        {
            const uint8_t APC_PHYSICAL_ID_LEN = static_cast<uint8_t>(DescriptionIndexing::APC_SEGMENTPOOL_END_SLAB) - static_cast<uint8_t>(DescriptionIndexing::APC_INDEX) + 1;
            const uint8_t offset_identity = 1u;

            using IAB = InstallAxisToBuffer;
            IAB::BuildNullIdentityBuffer(identity);
            if (!ValidateADescriptionBuffer(description))
            {
                return false;
            }

            for (uint8_t i = 0; i < APC_PHYSICAL_ID_LEN; i++)
            {
                identity[i + offset_identity] = description[i];
            }

            IAB::GraphMutationValues values{};
            values.Flags = static_cast<uint32_t>(IAB::MemGraphFlag::LIVE);
            values.SeqLockVertical = UNSIGNED_ZERO;
            values.SeqLockHorizontal = UNSIGNED_ZERO;


            return 
                IAB::InsertGraphIdentityMutation(identity, values) &&
                IAB::SealIdentityBuffer(identity);
        }

    };


}