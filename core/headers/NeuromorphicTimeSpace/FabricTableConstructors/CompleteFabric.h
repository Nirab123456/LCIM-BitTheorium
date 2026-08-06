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
    
        std::optional<size_t> GetIdStateIdxByDescriptionIdx_(uint64_t description_idx) noexcept;

        bool ReadACompleateAPCDescriptorBuffer_(
            uint64_t apc_description_index, 
            DescriptionOfAPC::SingleAPCDescriptionCellBuffer& return_buffer
        ) noexcept;

        /// @brief UPDATES: A Description In ONE SHOT
        bool OneShotUpdateReservedDescription_(
            DescriptionOfAPC::SingleAPCDescriptionCellBuffer& a_valid_description_buffer
        ) noexcept;

        DescriptionOfAPC::DescriptorSaftyFiles ReadAPCStateAtomically_(uint64_t apc_description_index) noexcept;

    public:
        APCSegmentPoolRange GetSegmentPoolBegainEndForSingleAPCDescription(uint64_t single_description_index) noexcept;

        /// @return previous ID_STATE -> raw value for reverting safely 
        std::optional<uint64_t> SwitchOwnershipOfAReadyDescription(
            uint64_t description_idx,
            DescriptionOfAPC::StateOfAPC updated_state,
            bool caller_holds_reservation = false,
            uint32_t max_tries = DEFAULT_MAX_TRIES
        ) noexcept;

        std::optional<uint64_t> GetASlotForNewAPCLink(uint64_t& desired_slot) noexcept;        
    };

    class EdgeTableConstructor : public APCHandleDescriptorConstructor
    {
    protected:
        using EdgeTableRange = DescriptorConf::APCDescriptorRange;

        EdgeTableRange ReadAnEdgeTableRange_(
            FabricSegments edge_table,
            uint32_t edge_idx
        ) noexcept;

        void InitializeEdgeTable_(FabricSegments edge_table) noexcept;

        bool ReadAnEdgeBuffer_(
            FabricSegments edge_table,
            uint32_t edge_idx,
            EdgeBuilder::EdgeBuffer& return_buffer
        ) noexcept;

        EdgeBuilder::EdgeStatus ReadEdgeData_(
            FabricSegments edge_table,
            uint32_t edge_idx,
            EdgeBuilder::EdgeData& edge_data,
            EdgeBuilder::EdgeBuffer* edge_buffer_return = nullptr
        ) noexcept;

    };



}