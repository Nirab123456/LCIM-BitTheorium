
#pragma once 
#include <array>
#include <utility>
#include "APCEnums.h"

namespace PredictedAdaptedEncoding
{


    struct LayoutHeaderIdentityOrchestrator
    {
        static constexpr bool IsValidTrackedAPCNode(MacroColumnOfAPC layout_node) noexcept
        {
            if (
                layout_node > MacroColumnOfAPC::NONE &&
                layout_node < MacroColumnOfAPC::META_HEADER
            )
            {
                return true;
            }
            return false;
        }

        static constexpr uint8_t LayoutBufferBegainInMetaIndecies() noexcept
        {
            return static_cast<uint8_t>(HeaderIdentifierOfAPC::FEEDFORWARD_BOUNDS);
        }

        static constexpr uint8_t LayoutBufferEndInMetaIndecies() noexcept
        {
            return static_cast<uint8_t>(HeaderIdentifierOfAPC::FREE_BOUNDS);
        }

    };

    struct ColumnConf : public LayoutHeaderIdentityOrchestrator
    {
    protected:
        static constexpr uint8_t RegionOrdinal(MacroColumnOfAPC macro_column) noexcept
        {
            return static_cast<uint8_t>(macro_column) - static_cast<uint8_t>(MacroColumnOfAPC::FEEDFORWARD_MESSAGE);
        }
    
    public:
        static constexpr bool IsTrackedRegionMacroColumn(MacroColumnOfAPC macro_column) noexcept
        {
            return macro_column >= MacroColumnOfAPC::FEEDFORWARD_MESSAGE &&
                macro_column <= MacroColumnOfAPC::FREE_SLOT;
        }

        static constexpr std::optional<HeaderIdentifierOfAPC> EnqueueHeaderIndexFromColumnName(MacroColumnOfAPC macro_column) noexcept
        {
            if (!IsTrackedRegionMacroColumn(macro_column))
            {
                return std::nullopt;
            }
            return static_cast<HeaderIdentifierOfAPC>(
                static_cast<uint8_t>(HeaderIdentifierOfAPC::FEEDFORWARD_ENQUEUE_POSITION) + RegionOrdinal(macro_column)
            );
        }

        static constexpr std::optional<HeaderIdentifierOfAPC> DequeueHeaderIndexFromColumnName(MacroColumnOfAPC macro_column) noexcept
        {
            if (!IsTrackedRegionMacroColumn(macro_column))
            {
                return std::nullopt;
            }
            return static_cast<HeaderIdentifierOfAPC>(
                static_cast<uint8_t>(HeaderIdentifierOfAPC::FEEDFORWARD_DEQUEUE_POSITION) + RegionOrdinal(macro_column)
            );
        }

        static constexpr std::optional<HeaderIdentifierOfAPC> SchemaHeaderIndexFromColumnName(MacroColumnOfAPC macro_column) noexcept
        {
            if (!IsTrackedRegionMacroColumn(macro_column))
            {
                return std::nullopt;
            }
            return static_cast<HeaderIdentifierOfAPC>(
                static_cast<uint8_t>(HeaderIdentifierOfAPC::FEEDFORWARD_REGION_SCHEMA) + RegionOrdinal(macro_column)
            );
        }

        static constexpr uint8_t CountOfMacroColumn() noexcept
        {
            return static_cast<uint8_t>(MacroColumnOfAPC::FREE_SLOT) - static_cast<uint8_t>(MacroColumnOfAPC::FEEDFORWARD_MESSAGE) + 1;
        }

    };

    struct APCDataStructure : public ColumnConf
    {

        static constexpr size_t METACELL_COUNT = 96;
        static constexpr uint32_t BRANCH_MAGIC = 0x41504342u;//big-endian
        static constexpr uint32_t EOF_HEADER = 0x72616600;//big-endian
        static constexpr uint8_t BRANCH_VERSION = 1u;
        static constexpr uint32_t APC_INDEX_BOUND_SENTINAL = UINT32_MAX;
        // static constexpr uint32_t APC_ALL_INDEX_LIMIT = APC_INDEX_BOUND_SENTINAL - 1;
        static constexpr size_t APC_CACHELINE_SIZE = 64u;


        static constexpr bool IsValidControlAPCUnit(uint64_t index) noexcept
        {
            return index < APC_INDEX_BOUND_SENTINAL;
        }

        static constexpr bool IsValidFabricUnit(uint64_t index) noexcept
        {
            return index < FABRIC_CELL_SENTINAL;
        }

        static constexpr bool InLimitOfUint8(uint32_t version) noexcept
        {
            return version < UINT8_MAX &&
                version > UNSIGNED_ZERO;
        }

        static constexpr bool IsCapacityOfAPCValid(uint32_t capacity) noexcept
        {
            return capacity >= MINIMUM_APC_CELL_COUNT &&
                IsValidControlAPCUnit(capacity);
        }

        static constexpr bool IsPowerOfTwoValue(uint64_t value) noexcept
        {
            return value != UNSIGNED_ZERO && (value & (value - 1u)) == UNSIGNED_ZERO;
        }


    protected:
            static constexpr void FreeAlignedRawPackedCells_(uint64_t* backing_ptr) noexcept
            {
                if (!backing_ptr)
                {
                    return;
                }
                ::operator delete[](static_cast<void*>(backing_ptr), std::align_val_t{APC_CACHELINE_SIZE});
            }
    };
}