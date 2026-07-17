#pragma once 
#include "CoreOfFabricCoordinator.hpp"

namespace PredictedAdaptedEncoding
{

    struct DescriptorConf : public EnumsOfFabricCoordinator
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
            RETIRED_OR_TOMBSTONE = 3,
            UNASSIGNED_UNUSED_NANNULL = 4
        };

        struct DescriptorSaftyFiles
        {
            uint32_t DescriptionID = UINT32_MAX;
            StateOfAPC StateOfTheAPC = StateOfAPC::UNASSIGNED_UNUSED_NANNULL;
            bool IsValid = false;
        };
        static_assert(sizeof(DescriptorSaftyFiles) <= sizeof(uint64_t));

        static constexpr bool IsKnownStateOfAPC(StateOfAPC state) noexcept
        {
            if (state < StateOfAPC::UNASSIGNED_UNUSED_NANNULL)
            {
                return true;
            }
            return false;
        }




        static constexpr uint64_t ComposeOwnershipAndLock(uint32_t description_id, StateOfAPC apc_state) noexcept
        {
            return Double32In64ExPa::PackDoubleUnsigned32In64(description_id, static_cast<uint32_t>(apc_state));
        }

        static constexpr DescriptorSaftyFiles GetDescriptioLockAndOwnership(uint64_t desc_id_state) noexcept
        {
            DescriptorSaftyFiles return_safty_files{};

            const std::optional<uint32_t> description_id_maybe = Double32In64ExPa::ExtractLow32Of64(desc_id_state);
            const std::optional<uint32_t> ownership_maybe = Double32In64ExPa::ExtractHigh32Of64(desc_id_state);

            if (
                !description_id_maybe.has_value() ||
                !ownership_maybe.has_value() 
            )
            {
                return return_safty_files;
            }

            return_safty_files.DescriptionID = description_id_maybe.value();
            return_safty_files.StateOfTheAPC = static_cast<StateOfAPC>(ownership_maybe.value());

            if (
                !IsKnownStateOfAPC(return_safty_files.StateOfTheAPC)
            )
            {
                return return_safty_files;
            }
            
            return_safty_files.IsValid = true;

            return return_safty_files;

        }

    };




    struct DescriptionOfAPC : DescriptorConf
    {

        static constexpr uint64_t VALID_BUFFER_MARK = 1111111111111;

        using SingleAPCDescriptionCellBuffer = std::array<uint64_t, APC_DESCRIPTOR_WIDTH_OR_VALIDATION_INDEX + 1>;


        static constexpr void BuildSentinalDescriptionBuffer(SingleAPCDescriptionCellBuffer& default_array)
        {
            for (size_t i = 0; i < default_array.size(); i++)
            {
                if (i < APC_DESCRIPTOR_WIDTH_OR_VALIDATION_INDEX)
                {
                    default_array[i] = FABRIC_CELL_SENTINAL;
                }
                else
                {
                    default_array[i] = UNSIGNED_ZERO;
                }
            }
        }

        static constexpr void BuildZerodDescriptionBuffer(SingleAPCDescriptionCellBuffer& default_array)
        {
            for (size_t i = 0; i < default_array.size(); i++)
            {
                default_array[i] = UNSIGNED_ZERO;
            }
        }

        // static constexpr bool ValidateADescriptionBuffer(
        //     SingleAPCDescriptionCellBuffer& desc_return_buff
        // ) noexcept;

        // static constexpr void SetADescriptionUnit(

        // )

        // static constexpr bool ConstructInitialAPCDescriptionBuffer(
        //     SingleAPCDescriptionCellBuffer& desc_return_buff,
        //     uint64_t apc_idx,
        //     uint64_t segment_pool_begin,
        //     uint64_t segment_pool_end,
        //     uint64_t next_apc_segment_pool = UNSIGNED_ZERO,
        //     StateOfAPC init_state = StateOfAPC::FREE_OR_EMPTY
        // ) noexcept
        // {
        //     BuildZerodDescriptionBuffer(desc_return_buff);

        // }


    };


}