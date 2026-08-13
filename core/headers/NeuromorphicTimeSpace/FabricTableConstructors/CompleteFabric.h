#pragma once 
#include "FabricConstructor.h"

namespace BidirectionalInMemGraph
{
    class RecordBookConstructor : public FabricConstructor
    {
        
    protected:
        using LBO = LayoutBoundsOrchestrator;
        using SD = SchemaDefinition;
        using IAB = InstallAxisToBuffer;
        using DSA = DescriptionOfAPC;
        using RBC = RecordBookConf;

        /// @return LOGICALLY AND SISTAMICALLY UINT64_MAX -> INVALID
        uint64_t GetStartingOfAnyFabricTable_(FabricSegments desired_table) noexcept;
        
        bool GetRecordMapCarrierRanges_(
            const FabricSegments table_class,
            RecordBookConf::RecordBookTablesBoundsCarrier& return_bounds
        ) noexcept;

        void IdleAFabricTableClassRangesMemory_(FabricSegments table_class) noexcept;

        void WriteARecordBookOfTSCEntry_(
            FabricSegments table_class, 
            size_t begin, 
            size_t end 
        ) noexcept;

    };


    class APCHandleDescriptorConstructor : public RecordBookConstructor
    {
    protected:

        DescriptorConf::APCDescriptorRange ReadAPCDescriptionRanges_(uint64_t apc_slot_index) noexcept;
    
        std::optional<size_t> GetDescriptionLockIdxInFabric_(uint64_t description_idx) noexcept;

        bool ReadACompleateAPCDescriptorBuffer_(
            uint64_t apc_description_index, 
            DescriptionOfAPC::SingleAPCDescriptionCellBuffer& return_buffer
        ) noexcept;

        /// @brief UPDATES: A Description In ONE SHOT
        bool OneShotUpdateReservedDescription_(
            DescriptionOfAPC::SingleAPCDescriptionCellBuffer& a_valid_description_buffer
        ) noexcept;

    public:
        RangeOfAPC GetSegmentPoolRange(uint64_t single_description_index) noexcept;

        /// @return previous ID_STATE -> raw value for reverting safely 
        bool SwitchDescriptionState(
            uint64_t description_idx,
            StateOfAPC updated_state,
            StateOfAPC desired_state,
            uint32_t max_tries = DEFAULT_MAX_TRIES
        ) noexcept;

        std::optional<uint32_t> GetASlotForNewAPCLink() noexcept;        

        DescriptionOfAPC::DescriptionLockValues ReadAPCStateAtomically_(uint64_t apc_description_index) noexcept;

    };

    class EdgeTableConstructor : public APCHandleDescriptorConstructor
    {
    public:
        using EdgeTableRange = DescriptorConf::APCDescriptorRange;

    private :
        bool SwitchEdgeState__(
            FabricSegments edge_table,
            uint32_t edge_idx,
            EdgeBuilder::EdgeData& pre_switch,
            EdgeBuilder::EdgeStatus desired_state,
            std::optional<EdgeBuilder::EdgeStatus> required_st = std::nullopt,
            uint32_t max_tries = DEFAULT_MAX_TRIES
        ) noexcept;

    protected:

        EdgeTableRange ReadAnEdgeTableRange_(
            FabricSegments edge_table,
            uint32_t edge_idx
        ) noexcept;

        void InitializeEdgeTable_(FabricSegments edge_table) noexcept;

        bool ReadAnEdgeBuffer_(
            FabricSegments edge_table,
            uint32_t edge_idx,
            EdgeBuilder::EdgeBuffer& return_buffer,
            uint32_t max_tries = DEFAULT_MAX_TRIES
        ) noexcept;

        std::optional<EdgeBuilder::EdgeStatus> ReadEdgeData_(
            FabricSegments edge_table,
            uint32_t edge_idx,
            EdgeBuilder::EdgeData& edge_data,
            EdgeBuilder::EdgeBuffer* edge_buffer_return = nullptr
        ) noexcept;

        bool ReserveAnEdge_(
            FabricSegments edge_table,
            uint32_t edge_idx,
            EdgeBuilder::EdgeData* pre_reserve_data = nullptr,
            std::optional<EdgeBuilder::EdgeStatus> expected_state = std::nullopt,
            uint32_t max_tries = DEFAULT_MAX_TRIES
        ) noexcept
        {
            EdgeBuilder::EdgeData local_before{};
            EdgeBuilder::EdgeData* before = pre_reserve_data ? pre_reserve_data : &local_before;
            return
                SwitchEdgeState__(
                    edge_table,
                    edge_idx,
                    *before,
                    EdgeBuilder::EdgeStatus::RESERVED,
                    expected_state,
                    max_tries
                );
        }

        bool PublishReservedEdge_(
            EdgeBuilder::EdgeData& desired_data,
            uint32_t edge_idx
        ) noexcept;

    };



}