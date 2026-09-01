#pragma once 
#include "FabricConstructor.h"
#include <new>

namespace BidirectionalInMemGraph
{
    class RecordBookConstructor : public APCHandleAndRetirement
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


    class APCLifeCycle : public RecordBookConstructor
    {
        friend class FabricToAPCLinker;
    protected:
    
        std::optional<uint64_t> GetDescriptionLockIdxInFabric_(uint64_t description_idx) noexcept;

        RangeOfAPC GetSegmentPoolRange(uint64_t single_description_index) noexcept;

        /// @return previous ID_STATE -> raw value for reverting safely 
        bool SwitchDescriptionState(
            uint64_t description_idx,
            StateOfAPC updated_state,
            StateOfAPC desired_state,
            uint32_t max_tries = DEFAULT_MAX_TRIES
        ) noexcept;

        DescriptionOfAPC::SeqLockAndStateStruct ReadAPCStateAtomically_(uint64_t apc_description_index) noexcept;

        void InitAllAPCLifeCycleState() noexcept;

    };

    class EdgeTableConstructor : public APCLifeCycle
    {
    public:
        using EdgeTableRange = RangeOfAPC;
        static constexpr uint8_t DEFAULT_DIRECTED_PARENT_PER_AXIS = 8u;

    protected:

        std::span<EdgeTableConf::ParentRelation> ParentRelation_(
            FabricSegments edge_table,
            uint32_t edge_idx
        ) noexcept;

        bool ConstructParentRelationObject_(
            FabricSegments edge_table,
            uint32_t edge_idx
        ) noexcept;

        EdgeTableRange ReadAnEdgeTableRange_(
            FabricSegments edge_table,
            uint32_t edge_idx
        ) noexcept;

        void InitializeEdgeTable_(FabricSegments edge_table) noexcept;

        SeqLockedOperation ReadEdgeData_(
            FabricSegments edge_table,
            uint32_t edge_idx,
            uint8_t relation_ordinal,
            EdgeBuilder::ParentRelation& relation,
            uint32_t max_tries = DEFAULT_MAX_TRIES
        ) noexcept;
    };


    



}