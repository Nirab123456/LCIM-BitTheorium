#pragma once 
#include "FabricTableOrchestrator.hpp"

namespace PredictedAdaptedEncoding
{

    struct DescriptorConf : public CoreOfFabricCoordinator
    {

        struct APCDescriptorRange
        {
            size_t BeginIndex = UNSIGNED_ZERO;
            size_t EndIndex = UNSIGNED_ZERO;
            bool IsValid = false;
        };

        enum class StateOfAPC : uint8_t
        {
            FREE_OR_EMPTY = 0,
            RESERVED = 1,
            LIVE_OR_PUBLISHED = 2,
            RETIRED_OR_TOMBSTONE = 3
        };
        static constexpr uint8_t LEN_OF_DESCRIPTION_AND_HASH_STATE = static_cast<uint8_t>(StateOfAPC::RETIRED_OR_TOMBSTONE) + 1;

        struct DescriptorSaftyFiles
        {
            uint32_t DescriptionID = UINT32_MAX;
            StateOfAPC StateOfTheAPC = StateOfAPC::RETIRED_OR_TOMBSTONE;
            bool IsValid = false;
        };
        static_assert(sizeof(DescriptorSaftyFiles) <= sizeof(uint64_t));

        static constexpr uint64_t ComposeIdAndState(uint32_t description_id, StateOfAPC apc_state) noexcept
        {
            if (
                !APCDataStructure::IsValid32BitAPCUnit(description_id)
            )
            {
                return FABRIC_CELL_SENTINAL;
            }
            return TwinU32ToU64::PackDoubleUnsigned32In64(description_id, static_cast<uint32_t>(apc_state));
        }

        static constexpr DescriptorSaftyFiles GetDescriptionFile(uint64_t desc_id_state) noexcept
        {
            DescriptorSaftyFiles return_safty_files{};
            const uint32_t description_id_maybe = TwinU32ToU64::ExtractLow32Of64(desc_id_state);
            const uint32_t ownership_maybe = TwinU32ToU64::ExtractHigh32Of64(desc_id_state);
            if (
                !APCDataStructure::IsValidFabricUnit(desc_id_state) ||
                !APCDataStructure::IsValid32BitAPCUnit(description_id_maybe) ||
                ownership_maybe >= LEN_OF_DESCRIPTION_AND_HASH_STATE
            )
            {
                return return_safty_files;
            }
            return_safty_files.DescriptionID = description_id_maybe;
            return_safty_files.StateOfTheAPC = static_cast<StateOfAPC>(ownership_maybe);
            return_safty_files.IsValid = true;
            return return_safty_files;
        }

        static constexpr bool IsTransitionStateLeagal(StateOfAPC current_state, StateOfAPC desired_state) noexcept
        {
            return (current_state == StateOfAPC::FREE_OR_EMPTY && desired_state == StateOfAPC::RESERVED) ||
                (current_state == StateOfAPC::RESERVED && desired_state == StateOfAPC::FREE_OR_EMPTY) ||
                (current_state == StateOfAPC::RESERVED && desired_state == StateOfAPC::LIVE_OR_PUBLISHED) ||
                (current_state == StateOfAPC::LIVE_OR_PUBLISHED && desired_state == StateOfAPC::RESERVED) ||
                (current_state == StateOfAPC::RESERVED && desired_state == StateOfAPC::RETIRED_OR_TOMBSTONE) ||
                (current_state == StateOfAPC::RETIRED_OR_TOMBSTONE && desired_state == StateOfAPC::RESERVED);
                
        }

    };

    struct DescriptionBuffer : public DescriptorConf
    {
        static constexpr uint64_t VALID_DESCRIPTION_BUFFER_MARK = 1111111111111ull;

        using SingleAPCDescriptionCellBuffer = std::array<uint64_t, DESCRIPTION_WIDTH_AND_VALIDATION_IDX + 1>;

        static constexpr void BuildSentinalDescriptionBuffer(SingleAPCDescriptionCellBuffer& default_array) noexcept
        {
            for (size_t i = 0; i < default_array.size(); i++)
            {
                if (i < DESCRIPTION_WIDTH_AND_VALIDATION_IDX)
                {
                    default_array[i] = FABRIC_CELL_SENTINAL;
                }
                else
                {
                    default_array[i] = UNSIGNED_ZERO;
                }
            }
        }

        static constexpr void BuildZerodDescriptionBuffer(SingleAPCDescriptionCellBuffer& default_array) noexcept
        {
            for (size_t i = 0; i < default_array.size(); i++)
            {
                default_array[i] = UNSIGNED_ZERO;
            }
        }

        static constexpr uint32_t ComposeDescriptionId(
            const SingleAPCDescriptionCellBuffer& description_buffer,
            StateOfAPC state
        ) noexcept
        {
            
            uint32_t hash = HASH32_GRATIO_1 * static_cast<uint32_t>(state);

            for (size_t i = 0; i < DESCRIPTION_WIDTH_AND_VALIDATION_IDX - 1; i++)
            {
                const uint64_t unit = description_buffer[i];

                hash ^= static_cast<uint32_t>(i);
                hash *= HASH32_GRATIO_2;

                hash ^= static_cast<uint32_t>(unit);
                hash *= HASH32_GRATIO_2;

                hash ^= static_cast<uint32_t>(unit >> (sizeof(uint32_t) * LEN_OF_BYTE_IN_BITS));
                hash *= HASH32_GRATIO_2;
            }
            
            if (hash == UNSIGNED_ZERO)
            {
                return 1u;
            }
            else if (hash == UINT32_MAX)
            {
                return hash - 1u;
            }
            return hash;
        }


    };
    

    struct DescriptionOfAPC : DescriptionBuffer
    {
        static constexpr bool ValidateADescriptionBuffer(
            SingleAPCDescriptionCellBuffer& desc_return_buff
        ) noexcept
        {
            for (size_t i = 0; i < DESCRIPTION_WIDTH_AND_VALIDATION_IDX; i++)
            {
                if (!APCDataStructure::IsValidFabricUnit(desc_return_buff[i]))
                {
                    desc_return_buff[DESCRIPTION_WIDTH_AND_VALIDATION_IDX] = UNSIGNED_ZERO;
                    return false;
                }
            }

            const uint64_t span_of_apc = desc_return_buff[static_cast<size_t>(DescriptionIdentity::APC_SEGMENTPOOL_END_SLAB)] -
                desc_return_buff[static_cast<size_t>(DescriptionIdentity::APC_SEGMENTPOOL_BEGAIN_SLAB)];
            
            const DescriptorSaftyFiles desc_files = GetDescriptionFile(
                desc_return_buff[static_cast<size_t>(DescriptionIdentity::ID_STATE_CONCURRENT)]
            );

            const uint32_t desc_id = ComposeDescriptionId(desc_return_buff, desc_files.StateOfTheAPC);

            if (
                !APCDataStructure::IsCapacityOfAPCValid(static_cast<uint32_t>(span_of_apc)) ||
                !desc_files.IsValid ||
                desc_id != desc_files.DescriptionID
            )
            {
                desc_return_buff[DESCRIPTION_WIDTH_AND_VALIDATION_IDX] = UNSIGNED_ZERO;
                return false;
            }
            
            desc_return_buff[DESCRIPTION_WIDTH_AND_VALIDATION_IDX] = VALID_DESCRIPTION_BUFFER_MARK;
            return true;
        }

        static constexpr void SetADescriptionUnit(
            SingleAPCDescriptionCellBuffer& desc_buffer,
            DescriptionIdentity identity,
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
            uint64_t segment_pool_end,
            uint64_t next_apc_segment_pool = UNSIGNED_ZERO,
            StateOfAPC init_state = StateOfAPC::FREE_OR_EMPTY
        ) noexcept
        {
            BuildZerodDescriptionBuffer(desc_return_buff);
            SetADescriptionUnit(desc_return_buff, DescriptionIdentity::APC_INDEX, apc_idx);
            SetADescriptionUnit(desc_return_buff, DescriptionIdentity::APC_SEGMENTPOOL_BEGAIN_SLAB, segment_pool_begin);
            SetADescriptionUnit(desc_return_buff, DescriptionIdentity::APC_SEGMENTPOOL_END_SLAB, segment_pool_end);
            SetADescriptionUnit(desc_return_buff, DescriptionIdentity::NEXT_SLOT_SEGMENTPOOL_BEGAIN, next_apc_segment_pool);

            const uint32_t desc_id = ComposeDescriptionId(desc_return_buff, init_state);
            const uint64_t id_state_unit = ComposeIdAndState(desc_id, init_state);
            if (
                !APCDataStructure::IsValid32BitAPCUnit(desc_id) ||
                !APCDataStructure::IsValidFabricUnit(id_state_unit)
            )
            {
                desc_return_buff[DESCRIPTION_WIDTH_AND_VALIDATION_IDX] = UNSIGNED_ZERO;
                return false;
            }
            
            SetADescriptionUnit(desc_return_buff, DescriptionIdentity::ID_STATE_CONCURRENT, id_state_unit);

            return ValidateADescriptionBuffer(desc_return_buff);
        }


        static constexpr bool HasValidationDescriptionMark(
            const SingleAPCDescriptionCellBuffer& desc_buffer
        ) noexcept
        {
            if (
                desc_buffer[DESCRIPTION_WIDTH_AND_VALIDATION_IDX] == VALID_DESCRIPTION_BUFFER_MARK
            )
            {
                return true;
            }
            return false;
        }


        static constexpr bool BuildIdentityBufferFromDescriptionBuffer(
            SingleAPCDescriptionCellBuffer& description,
            InstallAxisToBuffer::BufferOfAPCIdentity& identity
        ) noexcept
        {
            const uint8_t APC_PHYSICAL_ID_LEN = static_cast<uint8_t>(DescriptionIdentity::APC_SEGMENTPOOL_END_SLAB) - static_cast<uint8_t>(DescriptionIdentity::APC_INDEX) + 1;
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
            return IAB::SealIdentityBuffer(identity);
        }

    };


}