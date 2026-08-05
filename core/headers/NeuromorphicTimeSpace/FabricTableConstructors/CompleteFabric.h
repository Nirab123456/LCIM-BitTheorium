#pragma once 
#include "FabricConstructor.h"

namespace PredictedAdaptedEncoding
{
    class RecordBookConstructor : public FabricConstructor
    {
        
    protected:
        using LBO = LayoutBoundsOrchestrator;
        using SD = SchemaDefinition;
        using IAB = InstallAxisToBuffer;
        using DSA = DescriptionOfAPC;

        /// @return LOGICALLY AND SISTAMICALLY UINT64_MAX -> INVALID
        uint64_t GetStartingOfAnyFabricTable_(FabricTableSegmentClasses desired_table) noexcept;
        
        bool GetRecordMapCarrierRanges_(
            const FabricTableSegmentClasses table_class,
            RecordBookConf::RecordBookTablesBoundsCarrier& return_bounds
        ) noexcept;

        void IdleAFabricTableClassRangesMemory_(FabricTableSegmentClasses table_class) noexcept;

        void WriteARecordBookOfTSCEntry_(
            FabricTableSegmentClasses table_class, 
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



}