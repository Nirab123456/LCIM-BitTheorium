#pragma once
#include <functional>
#include "../APCOrchestrators/ViewOrchestrator.hpp"

namespace BidirectionalInMemGraph
{
    
    class VagueTemoraryPremativeFabric;
    class AdaptivePackedCellContainer;

    class FabricToAPCLinker 
    {
        friend class VagueTemoraryPremativeFabric;
    public:
        enum class SeqLockedOperation : uint8_t
        {
            FOUND = 0,
            NONE = 1,
            RETRY = 2
        };

        VagueTemoraryPremativeFabric* FabricOwnerPtr_{nullptr};
        RangeOfAPC RangeOfThisAPCInSlab_{};
        uint32_t CapacityOfThisAPC_{UNSIGNED_ZERO};
        std::byte* RawAPCBasePtr_{nullptr};
        
        uint32_t APCSlotIdx_{APCDataStructure::APC_INDEX_BOUND_SENTINAL};

        struct RelationOparation 
        {
            AdaptivePackedCellContainer* APCPtr_ = nullptr;
            SeqLockedOperation MutationOP_ = SeqLockedOperation::NONE;
        };

        void ReleseFabricBindingOnly_() noexcept;

        bool ForceCopyToAPCFromBuffer(
            uint32_t tarting_idx_in_apc,
            uint32_t sequential_number_of_cells,
            const uint64_t* source_cells
        ) noexcept;

        bool CopyFromAPCToBuffer(
            uint32_t starting_idx_in_apc,
            uint32_t sequential_number_of_cells,
            uint64_t* return_buffer
        ) noexcept;

        bool BindExternalRawFabricBacking_(
            uint64_t* raw_cells_ptr,
            uint32_t cell_count,
            VagueTemoraryPremativeFabric* fabric_owner,
            uint64_t fabric_slot_idx
        ) noexcept;

        bool AtomicallyReadLongLongAPCUnit(
            uint64_t idx,
            uint64_t& return_value
        ) noexcept;

        void AtomicallyWriteU64ToAPC(
            uint64_t idx,
            const uint64_t& value
        ) noexcept;

        VagueTemoraryPremativeFabric* GetFabricOwner() noexcept
        {
            return FabricOwnerPtr_;
        }

        static constexpr uint32_t PayloadBegin() noexcept
        {
            return APCDataStructure::METACELL_COUNT;
        }

        bool IsThisAPCValid() noexcept
        {
            return
                FabricOwnerPtr_ &&
                RangeOfThisAPCInSlab_.IsValid;
        }

        uint32_t GetThisSlotIdx() noexcept
        {
            return APCSlotIdx_;
        }

        bool InitiateAPCMetaHeader(
            const LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier& user_defined_weight = LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier{},
            const SchemaDefinition::InitialRegionalDtypeConf& dtype_conf = SchemaDefinition::InitialRegionalDtypeConf{},
            const SchemaDefinition::InitialRegionalProtocol& protocol_conf = SchemaDefinition::InitialRegionalProtocol{},
            uint8_t version = APCDataStructure::BRANCH_VERSION
        ) noexcept;

        bool ReadAPCMetaUnit(
            HeaderIdentifierOfAPC meta_idx,
            uint64_t& return_value
        ) noexcept;



    };
        
    
}