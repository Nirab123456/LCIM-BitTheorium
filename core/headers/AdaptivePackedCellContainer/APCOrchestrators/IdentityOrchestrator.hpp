#pragma once 
#include <array>
#include <utility>
#include "../CoreOFAPC/IdAndIdentityOfAPC.hpp"
#include "../../SharedComponents/BitPackers/ConAndCaDependentPacker.hpp"


namespace PredictedAdaptedEncoding
{

    struct IdentityBufferConf : public AxisConstructor
    {
        static constexpr uint8_t IDENTITY_BUFFER_LEN = APCDataStructure::TotalIdentityUnitCount();

        using BufferOfAPCIdentity = std::array<uint64_t, IDENTITY_BUFFER_LEN>;


        static constexpr bool IsKnownIdentity(HeaderIdentifierOfAPC identity_unit) noexcept
        {
            return identity_unit >= HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT &&
                identity_unit <= HeaderIdentifierOfAPC::ACCESS_PASSWORD;
        }

        static constexpr std::optional<uint8_t> GetBufferIdxFromIdentityUnit(HeaderIdentifierOfAPC identity_unit) noexcept
        {
            if (!IsKnownIdentity(identity_unit))
            {
                return std::nullopt;
            }
            
            return static_cast<uint8_t>(
                static_cast<uint8_t>(identity_unit) + static_cast<uint8_t>(HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT)
            );
        }

        static constexpr std::optional<HeaderIdentifierOfAPC> GetIdentityUnitFromBufferIdx(uint8_t buffer_idx) noexcept
        {
            if (
                buffer_idx >= IDENTITY_BUFFER_LEN
            )
            {
                return std::nullopt;
            }
            return static_cast<HeaderIdentifierOfAPC>(
                buffer_idx - static_cast<uint8_t>(HeaderIdentifierOfAPC::IDENTITY_FINGERPRINT)
            );
        }

        static constexpr void BuildNullIdentityBuffer(BufferOfAPCIdentity& identity_buffer) noexcept
        {
            for (size_t i = 0; i < identity_buffer.size(); i++)
            {
                identity_buffer[i] = FABRIC_CELL_SENTINAL;
            }
        }

        static constexpr bool InsertAnIdentityInBuffer(
            BufferOfAPCIdentity& identity_buffer,
            HeaderIdentifierOfAPC identity,
            uint64_t value
        ) noexcept
        {
            const std::optional<uint8_t> buffer_idx = GetBufferIdxFromIdentityUnit(identity);
            if (
                !buffer_idx.has_value() ||
                !APCDataStructure::IsValidFabricUnit(value)
            )
            {
                return false;
            }

            identity_buffer[buffer_idx.value()] = value;
        }

        static constexpr uint64_t ValueOfAnIdentityFromBuffer(
            const BufferOfAPCIdentity& identity_buffer,
            HeaderIdentifierOfAPC identity
        ) noexcept
        {
            const std::optional<uint8_t> buffer_idx = GetBufferIdxFromIdentityUnit(identity);
            if (!buffer_idx.has_value())
            {
                return FABRIC_CELL_SENTINAL;
            }

            return identity_buffer[buffer_idx.value()];
        }


    };

    struct InstallAxisToBuffer : public IdentityBufferConf
    {
        // static constexpr bool PlaceholderAxisCreation(
        //     BufferOfAPCIdentity& identity_buffer,
        //     BidirectionalAxis axis
        // ) noexcept
        // {
        //     const uint64_t branch_id = ValueOfAnIdentityFromBuffer(identity_buffer, HeaderIdentifierOfAPC::BRANCH_ID);
        //     if (!IsValidAPCId(branch_id))
        //     {
        //         return false;
        //     }

        //     const AxisConstructionMap axis_construction_map = ConstructAxisMap(axis);
            
            
        // }
    };
    
    

}