#pragma once
#include "ConstructorsOfAPC/ReadAndWriteOfAPC.h"

namespace PredictedAdaptedEncoding
{


class SegmentIODefinition : public ReadAndWriteOfAPC
{
public:
    SegmentIODefinition() noexcept = default;
    
protected:

    bool TurnOnMultipleSegmentFlagsAtOnce_(uint32_t use_or_between_flags = UNSIGNED_ZERO) noexcept
    {
        return UpdateAPCModeFlagsInHeader_(use_or_between_flags, UNSIGNED_ZERO, MetaIndexOfAPCNode::SEGMENT_CONF_FLAGS);
    }

    bool UpdateAPCModeFlagsInHeader_(uint64_t flags_to_turn_on = UNSIGNED_ZERO, uint64_t flags_to_turn_off = UNSIGNED_ZERO, MetaIndexOfAPCNode desired_flag_idx = MetaIndexOfAPCNode::SEGMENT_CONF_FLAGS) noexcept;

    bool TurnOnReadyBitForDesiredPagedNode_(APCPagedNodeSegmentClasses desired_region_class) noexcept;

    bool ClearTheDesiredPagedNodeReadyBit_(APCPagedNodeSegmentClasses desired_region_class) noexcept;

    bool ClearMultipleControlFlags_(uint32_t use_or_between_flags = UNSIGNED_ZERO) noexcept
    {
        return UpdateAPCModeFlagsInHeader_(UNSIGNED_ZERO, use_or_between_flags);
    }

    bool ResetALLOccupancy16x3ModelToZero_() noexcept;

    /// @brief APC META USES TypeFamily::VALUE32 path WITH::AccessContractOfValue -> SYSTEM BROKES if anythhing else then32Bit family
    void InsertTypedValue48MetaCellOfAPC_(
        MetaIndexOfAPCNode idx,
        uint64_t value48,
        LocalityPolicy locality = LocalityPolicy::PUBLISHED
    ) noexcept;

    void WrireAPCMetaModel_48t(
        MetaIndexOfAPCNode idx,
        uint64_t raw48_value,
        Model48Subclass sub_class = Model48Subclass::SELF_CLASS,
        LocalityPolicy locality = LocalityPolicy::PUBLISHED,
        InternalDataTypePolicy dtype = InternalDataTypePolicy::UnsignedPCellDataType,
        AttributePolicy attribute = AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL
    ) noexcept;

private:

    bool CasUpdateOccupancy3x16ThreeSubdivisionCell__(
        LocalityPolicy from_locality,
        LocalityPolicy to_locality,
        std::optional<APCPagedNodeSegmentClasses> page_class = std::nullopt,
        LocalityPolicy control_cells_own_locality = LocalityPolicy::PUBLISHED,
        bool is_this_cell_central_occupancy_counter = false
    ) noexcept;

    
public:
    packed64_t PackPureClock48AsPackedCell(
        std::optional<uint64_t> clock48 = std::nullopt,
        AttributePolicy attribute = AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL,
        LocalityPolicy locality = LocalityPolicy::PUBLISHED,
        APCPagedNodeSegmentClasses page_class = APCPagedNodeSegmentClasses::META_HEADER
    ) noexcept;

    void WriteOrUpdateMetaClock48(AttributePolicy attribute = AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL, std::optional<uint64_t>meta_clock_48 = std::nullopt) noexcept;

    bool ReplaceOnlyMetaCellValue(
        MetaIndexOfAPCNode idx,
        uint64_t expected_value,
        uint64_t desired_value,
        bool refresh_clock16 = true
    ) noexcept;


    void ConfigureThisAPCIdentity(
        const APCGroupReserver::APCInitialIdentityStruct& container_configuration
    ) noexcept;

    void InitRootOrChildBranch(
        size_t total_capacity,
        const APCGroupReserver::APCInitialIdentityStruct& container_configuration
    ) noexcept;

    void InitNodeSemantics(
        uint64_t aux_param_uint48 = UNSIGNED_ZERO
    ) noexcept;

    uint64_t ReadValuFromAPCMetaIndecies(MetaIndexOfAPCNode idx) noexcept;

    void TouchLocalMetaClock48() noexcept;

    bool TryIncrementOrDecrementActiveThreadCount(int8_t change_count) noexcept;

    bool TryBindPortTarget(MetaIndexOfAPCNode port_meta_idx, uint64_t target_branch_id) noexcept;

    uint64_t TotalCASFailForThisBranchIncreaseAndGet(uint32_t increment) noexcept;

    bool SetLayOutBounds(
        APCPagedNodeSegmentClasses page_class, 
        uint16_t begain_index,
        uint16_t end_index,
        bool caller_already_holds_flag = false,
        std::optional<uint16_t> version_number = std::nullopt
    ) noexcept;

    bool TrySetLayoutMutationInFlight() noexcept;

    bool TryExtendInternalPagedNode(APCPagedNodeSegmentClasses desired_rel_mask, uint32_t wanted_amount, APCGroupReserver::APCInitialIdentityStruct::APCSegmentExtendOrder desired_apc_order) noexcept;

    clk16_t ReadLastAcceptedClok16ForThisSegment(APCPagedNodeSegmentClasses region_kind) noexcept;
    clk16_t ReadLastEmittedClok16ForThisSegment(APCPagedNodeSegmentClasses region_kind) noexcept;
    size_t PayloadCapacityFromHeader() noexcept;

    bool ApplyCentralAndRegionOccupancyTransitionCell(
        packed64_t old_cell,
        packed64_t new_cell,
        APCPagedNodeSegmentClasses physical_page_class = APCPagedNodeSegmentClasses::NULLNAN
    ) noexcept;

    bool RefreshReadyBitForRegionFromOccupancy(APCPagedNodeSegmentClasses page_class) noexcept;

    uint16_t ReadTotalOccuPancyOfAnyPageClass(APCPagedNodeSegmentClasses page_class = APCPagedNodeSegmentClasses::NULLNAN) noexcept;
    
    bool ForceAutoReplaceAPCMetaCellValue(MetaIndexOfAPCNode idx, uint64_t value) noexcept;

    bool ValidateAPCOccupancyInvarient() noexcept;

    bool TurnOnASegmentFlag(ControlEnumOfAPCSegment desired_segment_flag) noexcept
    {
        return UpdateAPCModeFlagsInHeader_(static_cast<uint32_t>(desired_segment_flag), UNSIGNED_ZERO, MetaIndexOfAPCNode::SEGMENT_CONF_FLAGS);
    }

    bool HasThisControlEnumFlag(ControlEnumOfAPCSegment flag) noexcept
    {
        return (ReadValuFromAPCMetaIndecies(MetaIndexOfAPCNode::SEGMENT_CONF_FLAGS) & static_cast<uint32_t>(flag)) != 0u;
    }

    bool ClearOneControlEnumFlagOfAPC(ControlEnumOfAPCSegment desired_control_flag) noexcept
    {
        return UpdateAPCModeFlagsInHeader_(UNSIGNED_ZERO, static_cast<uint32_t>(desired_control_flag), MetaIndexOfAPCNode::SEGMENT_CONF_FLAGS);
    }
    
    uint32_t GetTotalCapacityForThisAPC() noexcept
    {
        return static_cast<uint32_t>(ReadValuFromAPCMetaIndecies(MetaIndexOfAPCNode::TOTAL_CAPACITY_OF_THIS_SEGEMENT));
    }

    void MakeAPCBranchOwned() noexcept
    {
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::CURRENTLY_OWNED, 1u);
    }


    void ResetTotalCASFailureForThisBranch() noexcept
    {
        InsertTypedValue48MetaCellOfAPC_(MetaIndexOfAPCNode::TOTAL_CAS_FAILURE_FOR_THIS_APC_BRANCH, UNSIGNED_ZERO);
    }

    packed64_t ReadCentralAPCOccupancyCellForThisPagedNode() noexcept
    {
        return ReadFullMetaCell(MetaIndexOfAPCNode::COMBINED_OCCUPANCY_PUBLISHED_CLAIMED_FAULTY_3x16_48);
    }
    
    packed64_t ReadRegionOccupancyCombinedCell(APCPagedNodeSegmentClasses page_class) noexcept
    {
        return ReadFullMetaCell(
            APCAndPagedNodeHelpers::GetOccupancyMetIndexByRegionClass(page_class)
        );
    }

    uint16_t ReadCentralAPCOccupancyOfALocality(LocalityPolicy locality_type) noexcept;

    bool GetPublishedClaimedFaultyFromCentral(
        uint16_t& published_occupancy,
        uint16_t& claimed_occupancy,
        uint16_t& faulty_occupancy
    )
    {
        const packed64_t central_occupancy_cell = ReadCentralAPCOccupancyCellForThisPagedNode();
        const uint64_t raw48 = APCDataStructure::AutoExtractDataOfAValidAPCCell(central_occupancy_cell);
        bool ok = Subdevision16x3InternalMode48CellModel::ExtractLowMidHighFromMode48_(raw48, published_occupancy, claimed_occupancy, faulty_occupancy);
        return ok;
    }

    uint16_t ReadRegionOccupancyOfALocality(LocalityPolicy locality_type, APCPagedNodeSegmentClasses page_class) noexcept;

    uint16_t ReadPublishedOccupancyOfAPageClass(APCPagedNodeSegmentClasses page_class) noexcept
    {
        return ReadRegionOccupancyOfALocality(LocalityPolicy::PUBLISHED, page_class);
    }

    uint16_t ReadIdleOccupancyOfAPAgeClass(APCPagedNodeSegmentClasses page_class) noexcept
    {
        return ReadRegionOccupancyOfALocality(LocalityPolicy::IDLE, page_class);
    }


    uint16_t ReadClaimedOccupancyOfAPAgeClass(APCPagedNodeSegmentClasses page_class) noexcept
    {
        return ReadRegionOccupancyOfALocality(LocalityPolicy::CLAIMED, page_class);
    }
    

    uint16_t ReadFaultyOccupancyOfAPAgeClass(APCPagedNodeSegmentClasses page_class) noexcept
    {
        return ReadRegionOccupancyOfALocality(LocalityPolicy::FAULTY, page_class);
    }

    uint16_t ReadTotalUsedOccupancyOfARegion(APCPagedNodeSegmentClasses page_class) noexcept
    {
        return ReadPublishedOccupancyOfAPageClass(page_class) +
            ReadClaimedOccupancyOfAPAgeClass(page_class) +
            ReadFaultyOccupancyOfAPAgeClass(page_class);
    }

    
};

}
