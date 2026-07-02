#pragma once 
#include <array>
#include <utility>
#include "OccupancyOrchestrator.hpp"

namespace PredictedAdaptedEncoding
{



struct LayoutBoundsOfSingleRelNodeClass
{
    uint32_t BeginIndex = BIT_FAMILY_32_SENTINAL;
    uint32_t EndIndex = BIT_FAMILY_32_SENTINAL;
    APCPagedNodeSegmentClasses PAGE_LAYOUT_CLASS = APCPagedNodeSegmentClasses::NULLNAN;
    float InitialOrCurrentPercentage = 0u;
    uint16_t VersionNumber = 0u;

    static constexpr MetaIndexOfAPCNode GetLayoutCellMetaIndexForPageClass(
        APCPagedNodeSegmentClasses page_class
    ) noexcept
    {
        switch (page_class)
        {
            case APCPagedNodeSegmentClasses::FEEDFORWARD_MESSAGE:
                return MetaIndexOfAPCNode::FEEDFORWARD_BOUNDS_VERSION;

            case APCPagedNodeSegmentClasses::FEEDBACKWARD_MESSAGE:
                return MetaIndexOfAPCNode::FEEDBACKWARD_BOUNDS_VERSION;

            case APCPagedNodeSegmentClasses::LATERAL_MESAGE:
                return MetaIndexOfAPCNode::LATERAL_BOUNDS_VERSION;

            case APCPagedNodeSegmentClasses::STATE_SLOT:
                return MetaIndexOfAPCNode::STATE_BOUNDS_VERSION;

            case APCPagedNodeSegmentClasses::ERROR_SLOT:
                return MetaIndexOfAPCNode::ERROR_BOUNDS_VERSION;

            case APCPagedNodeSegmentClasses::EDGE_DESCRIPTOR:
                return MetaIndexOfAPCNode::EDGE_DESCRIPTIOR_BOUNDS_VERSION;

            case APCPagedNodeSegmentClasses::WEIGHT_SLOT:
                return MetaIndexOfAPCNode::WEIGHT_BOUNDS_VERSION;

            case APCPagedNodeSegmentClasses::AUX_SLOT:
                return MetaIndexOfAPCNode::AUX_BOUNDS_VERSION;

            case APCPagedNodeSegmentClasses::HETEROGENOUS_RAW_MEMORY:
                return MetaIndexOfAPCNode::HETEROGENOUS_RAW_MEMORY_BOUNDS_VERSION;
                
            case APCPagedNodeSegmentClasses::RAW_64BIT_MEMORY:
                return MetaIndexOfAPCNode::RAW_64Bit_MEMORY;

            case APCPagedNodeSegmentClasses::PAIRED_POINTER_IN_MEMORY:
                return MetaIndexOfAPCNode::PAIRED_POINTER_IN_MEMORY_BOUNDS_VERSION;

            case APCPagedNodeSegmentClasses::UNDEFINED:
                return MetaIndexOfAPCNode::UNDEFINED_BOUNDS_VERSION;

            case APCPagedNodeSegmentClasses::FREE_SLOT:
                return MetaIndexOfAPCNode::FREE_BOUNDS_VERSION;

            default:
                return MetaIndexOfAPCNode::EOF_APC_HEADER;
        }
    }


    constexpr void SetOrResetPercentage(uint32_t total_capacity_of_apc) noexcept
    {
        InitialOrCurrentPercentage = static_cast<float>((static_cast<float>(GetPayloadSpan()) / static_cast<float>(total_capacity_of_apc)) * 100.00);
    }

    constexpr bool IsValid(uint32_t payload_begain, uint32_t payload_end) const noexcept
    {
        return BeginIndex >= payload_begain && EndIndex >= BeginIndex && EndIndex <= payload_end && PAGE_LAYOUT_CLASS!= APCPagedNodeSegmentClasses::NULLNAN;
    }

    constexpr bool IsEmpty() const noexcept
    {
        return EndIndex <= BeginIndex || PAGE_LAYOUT_CLASS == APCPagedNodeSegmentClasses::NULLNAN;
    }

    constexpr uint32_t GetPayloadSpan() const noexcept
    {
        return (EndIndex > BeginIndex) ? (EndIndex - BeginIndex) : 0u;
    }

    constexpr bool CanBorrowRightFrom(const LayoutBoundsOfSingleRelNodeClass& right) const noexcept
    {
        return EndIndex == right.BeginIndex && right.GetPayloadSpan() > 0u && right.PAGE_LAYOUT_CLASS != APCPagedNodeSegmentClasses::NULLNAN;
    }

    constexpr bool CanBorrowLeftFrom(const LayoutBoundsOfSingleRelNodeClass& left) const noexcept
    {
        return BeginIndex == left.EndIndex && left.GetPayloadSpan() > 0u && left.PAGE_LAYOUT_CLASS != APCPagedNodeSegmentClasses::NULLNAN;
    }

    constexpr bool TryGrowRight(uint32_t amount, LayoutBoundsOfSingleRelNodeClass& right) noexcept
    {
        if (!CanBorrowRightFrom(right) || amount == 0u || right.GetPayloadSpan() < amount)
        {
            return false;
        }
        EndIndex +=amount;
        right.BeginIndex +=amount;
        return true;            
    }

    constexpr bool TryGrowLeft(uint32_t amount, LayoutBoundsOfSingleRelNodeClass& left) noexcept
    {
        if (!CanBorrowLeftFrom(left) || amount == 0u || left.GetPayloadSpan() < amount)
        {
            return false;
        }
        BeginIndex -= amount;
        left.EndIndex -= amount;
        return true;
    }

    constexpr uint32_t ClampOrNormalize(uint32_t idx) const noexcept
    {
        if (IsEmpty())
        {
            return BeginIndex;
        }
        if (idx < BeginIndex || idx >= EndIndex)
        {
            return BeginIndex;
        }
        return BeginIndex + ((idx - BeginIndex) % GetPayloadSpan());
    }

    constexpr uint32_t ComputeWantedSpanFromTotal(uint32_t total_payload_span) const noexcept
    {
        return (static_cast<uint32_t>(InitialOrCurrentPercentage) * total_payload_span) / 100u;
    }

    constexpr bool DoseThisIndexPhysicallyExistInThisRegion(size_t index) const noexcept
    {
        return index >= BeginIndex && index < EndIndex;
    }
    
    constexpr bool CanCellBEConsumedForThisPhysicalRegion(
        packed64_t packed_cell,
        size_t idx
    ) noexcept
    {
        if (!DoseThisIndexPhysicallyExistInThisRegion(idx))
        {
            return false;
        }

        const PackedCell64_t::AuthoritiveCellView a_cell_view = PackedCell64_t::GetAuthoritiveViewsForACell(packed_cell);

        if (!a_cell_view.IsCellValid)
        {
            return false;
        }

        if (a_cell_view.PageClass != PAGE_LAYOUT_CLASS)
        {
            return false;
        }
        
        if (APCAndPagedNodeHelpers::IsEmbededControlCell(a_cell_view) || APCAndPagedNodeHelpers::IsEmbededTimerCell(a_cell_view))
        {
            return false;
        }

        if (!APCAndPagedNodeHelpers::IsDataConsumablePageClass(a_cell_view.PageClass))
        {
            return false;
        }
        
        return true;
    }

    static constexpr bool ExtractLayoutModel_BegainL_EndM_VersionH(packed64_t packed_cell, uint16_t& begin_index, uint16_t& end_index, uint16_t& version_count) noexcept
    {
        if (!Subdevision16x3InternalMode48CellModel::IsThisCellASubdevision_3x16_48t(packed_cell))
        {
            return false;
        }

        const uint64_t raw48 = APCDataStructure::AutoExtractDataOfAValidAPCCell(packed_cell, true);
        
        return Subdevision16x3InternalMode48CellModel::ExtractLowMidHighFromMode48_(raw48, begin_index, end_index, version_count);
    }

};

struct CompleteAPCNodeRegionsLayout
{

    static constexpr LayoutBoundsOfSingleRelNodeClass MakeDefaultDesiredLayout(
        APCPagedNodeSegmentClasses desired_layout_class,
        uint8_t initial_percentage
    ) noexcept
    {
        return LayoutBoundsOfSingleRelNodeClass{
            BIT_FAMILY_32_SENTINAL,
            BIT_FAMILY_32_SENTINAL,
            desired_layout_class,
            static_cast<float>(initial_percentage)
        };
    }

    LayoutBoundsOfSingleRelNodeClass FeedForwardLayout{MakeDefaultDesiredLayout(APCPagedNodeSegmentClasses::FEEDFORWARD_MESSAGE, FEEDFOEWARD_PERCENTAGE)};
    LayoutBoundsOfSingleRelNodeClass FeedBackwardLayout{MakeDefaultDesiredLayout(APCPagedNodeSegmentClasses::FEEDBACKWARD_MESSAGE, FEEDBACKWARD_PERCENTAGE)};
    LayoutBoundsOfSingleRelNodeClass LateralLayout{MakeDefaultDesiredLayout(APCPagedNodeSegmentClasses::LATERAL_MESAGE, UNSIGNED_ZERO)};
    LayoutBoundsOfSingleRelNodeClass StateLayout{MakeDefaultDesiredLayout(APCPagedNodeSegmentClasses::STATE_SLOT, STATESLOT_PERCENTAGE)};
    LayoutBoundsOfSingleRelNodeClass ErrorLayout{MakeDefaultDesiredLayout(APCPagedNodeSegmentClasses::ERROR_SLOT, ERRORSLOT_PERCENTAGE)};
    LayoutBoundsOfSingleRelNodeClass EdgeDescriptorLayout{MakeDefaultDesiredLayout(APCPagedNodeSegmentClasses::EDGE_DESCRIPTOR, EDGEDESCRIPTOR_PERCENTAGE)};
    LayoutBoundsOfSingleRelNodeClass WeightLayout{MakeDefaultDesiredLayout(APCPagedNodeSegmentClasses::WEIGHT_SLOT, WEIGHTSLOT_PERCENTAGE)};
    LayoutBoundsOfSingleRelNodeClass AUXLayout{MakeDefaultDesiredLayout(APCPagedNodeSegmentClasses::AUX_SLOT, AUXSLOT_PERCENTAGE)};
    LayoutBoundsOfSingleRelNodeClass HeterogenousMemoryLayout{MakeDefaultDesiredLayout(APCPagedNodeSegmentClasses::HETEROGENOUS_RAW_MEMORY, UNSIGNED_ZERO)};
    LayoutBoundsOfSingleRelNodeClass LocalPairedPointerLayout{MakeDefaultDesiredLayout(APCPagedNodeSegmentClasses::RAW_64BIT_MEMORY, UNSIGNED_ZERO)};
    LayoutBoundsOfSingleRelNodeClass DistancePairedLayout{MakeDefaultDesiredLayout(APCPagedNodeSegmentClasses::PAIRED_POINTER_IN_MEMORY, UNSIGNED_ZERO)};
    LayoutBoundsOfSingleRelNodeClass UndefinedLayout{MakeDefaultDesiredLayout(APCPagedNodeSegmentClasses::UNDEFINED, UNSIGNED_ZERO)};
    LayoutBoundsOfSingleRelNodeClass FreeLayout{MakeDefaultDesiredLayout(APCPagedNodeSegmentClasses::FREE_SLOT, FREE_PERCENTAGE)};
    //we can add 8 more threrritically rel_mask = 4 bit ->16 classes 
    static constexpr uint8_t CURRENT_TOTAL_APC_REL_NODE_CLASSES = 13u;

    constexpr float SumOfPercentage() const noexcept
    {
        return FeedForwardLayout.InitialOrCurrentPercentage + FeedBackwardLayout.InitialOrCurrentPercentage + 
                LateralLayout.InitialOrCurrentPercentage + StateLayout.InitialOrCurrentPercentage +
                ErrorLayout.InitialOrCurrentPercentage + EdgeDescriptorLayout.InitialOrCurrentPercentage + 
                WeightLayout.InitialOrCurrentPercentage + AUXLayout.InitialOrCurrentPercentage + 
                HeterogenousMemoryLayout.InitialOrCurrentPercentage + LocalPairedPointerLayout.InitialOrCurrentPercentage +
                DistancePairedLayout.InitialOrCurrentPercentage + UndefinedLayout.InitialOrCurrentPercentage +
                FreeLayout.InitialOrCurrentPercentage;
    }

    constexpr bool DoseAllPhysicalLayoutCarrySameVersionNumberAsGlobal(
        uint16_t global_version_number
    ) noexcept
    {
        if (global_version_number == UNSIGNED_ZERO ||
            global_version_number == APCDataStructure::APC_INDEX_SENTINAL)
        {
            return false;
        }

        return
            FeedForwardLayout.VersionNumber == global_version_number &&
            FeedBackwardLayout.VersionNumber == global_version_number &&
            LateralLayout.VersionNumber == global_version_number &&
            StateLayout.VersionNumber == global_version_number &&
            ErrorLayout.VersionNumber == global_version_number &&
            EdgeDescriptorLayout.VersionNumber == global_version_number &&
            WeightLayout.VersionNumber == global_version_number &&
            AUXLayout.VersionNumber == global_version_number &&
            HeterogenousMemoryLayout.VersionNumber == global_version_number &&
            LocalPairedPointerLayout.VersionNumber == global_version_number &&
            DistancePairedLayout.VersionNumber == global_version_number &&
            UndefinedLayout.VersionNumber == global_version_number &&
            FreeLayout.VersionNumber == global_version_number;
    }
    
    constexpr bool NormalizePercentagesIfNeeded() noexcept
    {
        const float sum_of_default = SumOfPercentage();
        if (sum_of_default == 100.00)
        {
            return true;
        }
        if (sum_of_default == 0.00)
        {
            FreeLayout.InitialOrCurrentPercentage = 100.00;
            return true;
        }
        auto NormalizeOne = [sum_of_default](LayoutBoundsOfSingleRelNodeClass& one) noexcept
        {
            one.InitialOrCurrentPercentage = (one.InitialOrCurrentPercentage * 100) / sum_of_default;
        };
        
        NormalizeOne(FeedForwardLayout);
        NormalizeOne(FeedBackwardLayout);
        NormalizeOne(LateralLayout);
        NormalizeOne(StateLayout);
        NormalizeOne(ErrorLayout);
        NormalizeOne(EdgeDescriptorLayout);
        NormalizeOne(WeightLayout);
        NormalizeOne(AUXLayout);
        NormalizeOne(HeterogenousMemoryLayout);
        NormalizeOne(LocalPairedPointerLayout);
        NormalizeOne(DistancePairedLayout);
        NormalizeOne(UndefinedLayout);
        NormalizeOne(FreeLayout);

        float repaired_sum = SumOfPercentage();
        if (repaired_sum < 100)
        {
            FreeLayout.InitialOrCurrentPercentage = FreeLayout.InitialOrCurrentPercentage + (100 - repaired_sum);
        }
        return true;
    }

    constexpr LayoutBoundsOfSingleRelNodeClass* GetALayoutByRelMask(APCPagedNodeSegmentClasses desired_rel_mask) noexcept
    {
        switch (desired_rel_mask)
        {
            case APCPagedNodeSegmentClasses::FEEDFORWARD_MESSAGE:  return &FeedForwardLayout;
            case APCPagedNodeSegmentClasses::FEEDBACKWARD_MESSAGE: return &FeedBackwardLayout;
            case APCPagedNodeSegmentClasses::LATERAL_MESAGE     :  return &LateralLayout;
            case APCPagedNodeSegmentClasses::STATE_SLOT:           return &StateLayout;
            case APCPagedNodeSegmentClasses::ERROR_SLOT:           return &ErrorLayout;
            case APCPagedNodeSegmentClasses::EDGE_DESCRIPTOR:      return &EdgeDescriptorLayout;
            case APCPagedNodeSegmentClasses::WEIGHT_SLOT:          return &WeightLayout;
            case APCPagedNodeSegmentClasses::AUX_SLOT:             return &AUXLayout;
            case APCPagedNodeSegmentClasses::HETEROGENOUS_RAW_MEMORY:
                return &HeterogenousMemoryLayout;
            case APCPagedNodeSegmentClasses::RAW_64BIT_MEMORY: 
                return &LocalPairedPointerLayout;
            case APCPagedNodeSegmentClasses::PAIRED_POINTER_IN_MEMORY:
                return &DistancePairedLayout;
            case APCPagedNodeSegmentClasses::UNDEFINED:            return &UndefinedLayout;
            case APCPagedNodeSegmentClasses::FREE_SLOT:            return &FreeLayout;
            default:                                               return nullptr;
        }
    }

    std::array<LayoutBoundsOfSingleRelNodeClass*, CURRENT_TOTAL_APC_REL_NODE_CLASSES> OrderedViewsFIFO() noexcept
    {
        return {
            &FeedForwardLayout, &FeedBackwardLayout, 
            &LateralLayout, &StateLayout,  
            &ErrorLayout, &EdgeDescriptorLayout,
            &WeightLayout, &AUXLayout, 
            &HeterogenousMemoryLayout, &LocalPairedPointerLayout,
            &DistancePairedLayout, &UndefinedLayout,
            &FreeLayout
        };
    }

    static constexpr void BuildInitialLayoutPlan(
        CompleteAPCNodeRegionsLayout& full_layout,
        uint32_t capacity_of_the_apc,
        uint16_t desired_version = APCDataStructure::BRANCH_VERSION
    ) noexcept
    {
        const uint32_t payload_begain = APCDataStructure::METACELL_COUNT;
        const uint32_t payload_end = capacity_of_the_apc;
        const uint32_t total_span = payload_end - payload_begain;

        if (
            payload_end <= payload_begain || 
            !APCDataStructure::IsThisIndexValidForAPC(capacity_of_the_apc) || 
            total_span < MINIMUM_BRANCH_CAPACITY
        )
        {
            return;
        }
        full_layout.NormalizePercentagesIfNeeded();

        uint32_t initial_cursor = payload_begain;
        
        auto AssignOne = [&](LayoutBoundsOfSingleRelNodeClass& one) noexcept
        {
            if (!APCAndPagedNodeHelpers::IsTrackedOccupancyPageClass(one.PAGE_LAYOUT_CLASS))
            {
                one.BeginIndex = initial_cursor;
                one.EndIndex = initial_cursor;
                one.VersionNumber = desired_version;
                return;
            }

            if (one.PAGE_LAYOUT_CLASS == APCPagedNodeSegmentClasses::FREE_SLOT)
            {
                return;
            }
            
            one.BeginIndex = initial_cursor;
            one.VersionNumber = desired_version;
            uint32_t wanted_span = one.ComputeWantedSpanFromTotal(total_span);
            if (wanted_span == UNSIGNED_ZERO)
            {
                one.EndIndex = initial_cursor;
                return;
            }
            
            wanted_span = std::max<uint32_t>(wanted_span, MIN_REGION_SIZE);
            const uint32_t remaining = payload_end > initial_cursor ? (payload_end - initial_cursor) : UNSIGNED_ZERO;
            wanted_span = std::min<uint32_t>(wanted_span, remaining);
            one.EndIndex = initial_cursor + wanted_span;
            initial_cursor = one.EndIndex;
        };

        auto ordered = full_layout.OrderedViewsFIFO();
        for (auto* one : ordered)
        {
            if (!one)
            {
                return;
            }
            if (one->PAGE_LAYOUT_CLASS == APCPagedNodeSegmentClasses::FREE_SLOT)
            {
                continue;
            }
            AssignOne(*one);
        }
        
        full_layout.FreeLayout.BeginIndex = initial_cursor;
        full_layout.FreeLayout.EndIndex = payload_end;
        full_layout.FreeLayout.PAGE_LAYOUT_CLASS = APCPagedNodeSegmentClasses::FREE_SLOT;
        full_layout.FreeLayout.VersionNumber = desired_version;
    }
};

/// @brief // EVERYTHING TOP WILL BE REMOVED
struct LayoutBuilderAndValidator
{
    struct LayoutCarrier
    {
        uint16_t BeginIndex = APCDataStructure::APC_INDEX_SENTINAL;
        uint16_t EndIndex = APCDataStructure::APC_INDEX_SENTINAL;
        uint8_t Version = UINT8_MAX;
        APCPagedNodeSegmentClasses LayoutIdentity = APCPagedNodeSegmentClasses::NULLNAN;
        LocalityPolicy LocalityOfLayout = LocalityPolicy::UNASSIGNED_UNUSED_NANNULL;
        bool IsValid = false;
    };
    static_assert(sizeof(LayoutCarrier) <= sizeof(packed64_t));

    static constexpr bool ValidateALayoutCarrier(
        LayoutCarrier& a_layout,
        bool is_claimed_valid = true
    ) noexcept
    {
        if (a_layout.LocalityOfLayout == LocalityPolicy::UNASSIGNED_UNUSED_NANNULL)
        {
            return false;
        }
        if (!is_claimed_valid && a_layout.LocalityOfLayout == LocalityPolicy::CLAIMED)
        {
            return false;
        }
        
        if (
            APCDataStructure::IsThisIndexValidForAPC(a_layout.BeginIndex) &&
            APCDataStructure::IsThisIndexValidForAPC(a_layout.EndIndex) &&
            APCDataStructure::ThisVersionValid(a_layout.Version) &&
            a_layout.BeginIndex <= a_layout.EndIndex &&
            PageNodeOrchestrator::IsValidLayoutNode(a_layout.LayoutIdentity)
        )
        {   
            return true;
        }
        a_layout.IsValid = false;
        return false;
    }

    static constexpr packed64_t CreateALayoutBoundsCell(
        LayoutCarrier& a_layout
    ) noexcept
    {
        if (!ValidateALayoutCarrier(a_layout))
        {
            return PackedCell64_t::PACKED_CELL_SENTINAL;
        }

        const uint64_t raw48_layout = Subdivision2x16Plus2x8InternalMode48CellModel::Pack2x16Plus2x8UnsignedSubdivision_(
            a_layout.BeginIndex,
            a_layout.EndIndex,
            a_layout.Version,
            static_cast<uint8_t>(a_layout.LayoutIdentity)
        );

        return PackedCell64_t::MakeModeledAPCValidPackedCell(
            ModelFamily::MODEL48,
            static_cast<tag8_t>(Model48Subclass::FOUR_SUBDIVISION_2x16_AND_2x8),
            APCPagedNodeSegmentClasses::META_HEADER,
            a_layout.LocalityOfLayout,
            InternalDataTypePolicy ::UnsignedPCellDataType,
            AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL,
            raw48_layout
        );

    }

    static constexpr LayoutCarrier GetLayoutCarrierFromValidLayoutCell(
        packed64_t packed_cell,
        bool is_claimed_valid = true
    ) noexcept
    {
        LayoutCarrier return_carrier{};
        const PackedCell64_t::AuthoritiveCellView desired_auth_view = PackedCell64_t::GetAuthoritiveViewsForACell(packed_cell);
        
        if (
            !PageNodeOrchestrator::IsValidAPCHeaderCell(desired_auth_view) ||
            desired_auth_view.SubClassOfModel48 != Model48Subclass::FOUR_SUBDIVISION_2x16_AND_2x8
        )
        {
            return return_carrier;
        }
        
        return_carrier.BeginIndex = Subdivision2x16Plus2x8InternalMode48CellModel::ExtractLowestFirstLow16Bit0_(desired_auth_view.Raw48BitInCellData);
        return_carrier.EndIndex = Subdivision2x16Plus2x8InternalMode48CellModel::ExtractSecondLow16Bit1_(desired_auth_view.Raw48BitInCellData);
        return_carrier.Version = Subdivision2x16Plus2x8InternalMode48CellModel::ExtractHigh8Bit2_(desired_auth_view.Raw48BitInCellData);
        return_carrier.LayoutIdentity = static_cast<APCPagedNodeSegmentClasses>(Subdivision2x16Plus2x8InternalMode48CellModel::ExtractHighestHigh8Bit3_(desired_auth_view.Raw48BitInCellData));
        return_carrier.LocalityOfLayout = desired_auth_view.LocalityOfCell;

        ValidateALayoutCarrier(return_carrier, is_claimed_valid);
        return return_carrier;
    }


};


struct LayoutPercentageBuilder : public LayoutBuilderAndValidator
{

    struct LayoutSpanAndPercentageCarrier
    {
        uint16_t FeedForward = FEEDFOEWARD_PERCENTAGE;
        uint16_t FeedBackward = FEEDBACKWARD_PERCENTAGE;
        uint16_t Lateral = LATERAL_PERCENTAGE;
        uint16_t StateSlot = STATESLOT_PERCENTAGE;
        uint16_t ErrorSlot = ERRORSLOT_PERCENTAGE;
        uint16_t EdgeDescriptor = EDGEDESCRIPTOR_PERCENTAGE;
        uint16_t WeightSlot = WEIGHTSLOT_PERCENTAGE;
        uint16_t AUXSlot = AUXSLOT_PERCENTAGE;
        uint16_t HeterogenousSlot = HETEROGENOUS_RAW_PERCENTAGE;
        uint16_t Raw64BitSlot = RAW64_BIT_PERCENTAGE;
        uint16_t PairedPtr = PAIRED_POINTER_PERCENTAGE;
        uint16_t FreeSlot = FREE_PERCENTAGE;
        uint16_t UndefinedSlot = UNDEFINED_PERCENTAGE;
    };


    static constexpr LayoutSpanAndPercentageCarrier DEFAULT_LAYOUT_PERCENTAGE{};

    static constexpr std::optional<uint16_t> GetDefaultInitialPercentage(
        APCPagedNodeSegmentClasses layout_class,
        const LayoutSpanAndPercentageCarrier& user_defined_percentage = DEFAULT_LAYOUT_PERCENTAGE
    ) noexcept
    {
        if (!PageNodeOrchestrator::IsValidLayoutNode(layout_class))
        {
            return std::nullopt;
        }

        switch (layout_class)
        {
        case APCPagedNodeSegmentClasses::FEEDFORWARD_MESSAGE:
            return user_defined_percentage.FeedForward;
        case APCPagedNodeSegmentClasses::FEEDBACKWARD_MESSAGE:
            return user_defined_percentage.FeedBackward;
        case APCPagedNodeSegmentClasses::LATERAL_MESAGE:
            return user_defined_percentage.Lateral;
        case APCPagedNodeSegmentClasses::STATE_SLOT:
            return user_defined_percentage.StateSlot;
        case APCPagedNodeSegmentClasses::ERROR_SLOT:
            return user_defined_percentage.ErrorSlot;
        case APCPagedNodeSegmentClasses::EDGE_DESCRIPTOR:
            return user_defined_percentage.EdgeDescriptor;
        case APCPagedNodeSegmentClasses::WEIGHT_SLOT:
            return user_defined_percentage.WeightSlot;
        case APCPagedNodeSegmentClasses::AUX_SLOT:
            return user_defined_percentage.AUXSlot;
        case APCPagedNodeSegmentClasses::HETEROGENOUS_RAW_MEMORY:
            return user_defined_percentage.HeterogenousSlot;
        case APCPagedNodeSegmentClasses::RAW_64BIT_MEMORY:
            return user_defined_percentage.Raw64BitSlot;
        case APCPagedNodeSegmentClasses::PAIRED_POINTER_IN_MEMORY:
            return user_defined_percentage.PairedPtr;
        case APCPagedNodeSegmentClasses::FREE_SLOT:
            return user_defined_percentage.FreeSlot;
        case APCPagedNodeSegmentClasses::UNDEFINED:
            return user_defined_percentage.UndefinedSlot;
        default:
            return std::nullopt;
        }
        
    }

    static constexpr bool NormalizeLayoutPercentageToPayloadSpan(
        LayoutSpanAndPercentageCarrier& user_defined_percentage,
        const uint16_t& payload_span
    ) noexcept
    {
        if (
            payload_span == UNSIGNED_ZERO ||
            !APCDataStructure::IsThisIndexValidForAPC(payload_span)
        )
        {
            return false;
        }

        std::array<uint16_t*, PageNodeOrchestrator::GetLenOfLayoutConstructorInAPCHeader()> layout_fields_array
        {
            &user_defined_percentage.FeedForward,
            &user_defined_percentage.FeedBackward,
            &user_defined_percentage.Lateral,
            &user_defined_percentage.StateSlot,
            &user_defined_percentage.ErrorSlot,
            &user_defined_percentage.EdgeDescriptor,
            &user_defined_percentage.WeightSlot,
            &user_defined_percentage.AUXSlot,
            &user_defined_percentage.HeterogenousSlot,
            &user_defined_percentage.Raw64BitSlot,
            &user_defined_percentage.PairedPtr,
            &user_defined_percentage.FreeSlot,
            &user_defined_percentage.UndefinedSlot
        };

        uint32_t total_weight = UNSIGNED_ZERO;
        for (size_t i = 0; i < PageNodeOrchestrator::GetLenOfLayoutConstructorInAPCHeader(); i++)
        {
            total_weight += static_cast<uint32_t>(*layout_fields_array[i]);
        }

        if (total_weight == UNSIGNED_ZERO)
        {
            return false;
        }

        std::array<uint16_t, PageNodeOrchestrator::GetLenOfLayoutConstructorInAPCHeader()> normalize_counts_array{};
        std::array<uint32_t, PageNodeOrchestrator::GetLenOfLayoutConstructorInAPCHeader()> reminders_array{};

        uint32_t assigned_count = UNSIGNED_ZERO;
        for (size_t i = 0; i < PageNodeOrchestrator::GetLenOfLayoutConstructorInAPCHeader(); i++)
        {
            const uint32_t weight = static_cast<uint32_t>(*layout_fields_array[i]);
            const uint32_t scaled = weight * static_cast<uint32_t>(payload_span);

            const uint32_t base_count = scaled / total_weight;
            const uint32_t reminder = scaled % total_weight;

            normalize_counts_array[i] = static_cast<uint16_t>(base_count);
            reminders_array[i] = reminder;
            assigned_count += base_count;
        }

        if (assigned_count > payload_span)
        {
            return false;
        }

        uint32_t remaining_cells  = static_cast<uint32_t>(payload_span) - assigned_count;

        while (remaining_cells > UNSIGNED_ZERO)
        {
            size_t best_idx = PageNodeOrchestrator::GetLenOfLayoutConstructorInAPCHeader();
            uint32_t best_reminder = UNSIGNED_ZERO;
            for (size_t i = 0; i < PageNodeOrchestrator::GetLenOfLayoutConstructorInAPCHeader(); i++)
            {
                if (
                    reminders_array[i] > best_reminder ||
                    (
                        reminders_array[i] == best_reminder && 
                        best_idx == PageNodeOrchestrator::GetLenOfLayoutConstructorInAPCHeader() &&
                        *layout_fields_array[i]
                    )
                )
                {
                    best_reminder = reminders_array[i];
                    best_idx = i;
                }
            }

            if (best_idx == PageNodeOrchestrator::GetLenOfLayoutConstructorInAPCHeader())
            {
                return false;
            }
            ++normalize_counts_array[best_idx];
            reminders_array[best_idx] = UNSIGNED_ZERO;
            --remaining_cells;
        }
        
        uint32_t final_sum = UNSIGNED_ZERO;

        for (size_t i = 0; i < PageNodeOrchestrator::GetLenOfLayoutConstructorInAPCHeader(); i++)
        {
            final_sum += normalize_counts_array[i];
        }

        if (final_sum != payload_span)
        {
            return false;
        }
        
        
        for (size_t i = 0; i < PageNodeOrchestrator::GetLenOfLayoutConstructorInAPCHeader(); i++)
        {
            *layout_fields_array[i] = normalize_counts_array[i];
        }
        return true;
    }


};



struct LayoutBoundsOrchestrator : public LayoutPercentageBuilder
{

    /// @brief ///////////////////////////////
    static constexpr uint8_t LEN_OF_LAYOUT_BUFFER = PageNodeOrchestrator::GetLenOfLayoutConstructorInAPCHeader() + 1;
    using LayoutBufferOfAPC = std::array<packed64_t, LEN_OF_LAYOUT_BUFFER>;


    static constexpr void BuildNullLayoutBuffer(LayoutBufferOfAPC& a_layout_buffer) noexcept
    {
        for (size_t i = 0; i < a_layout_buffer.size(); i++)
        {
            a_layout_buffer[i] = PackedCell64_t::PACKED_CELL_SENTINAL;
        }
    }

    static constexpr std::optional<uint8_t> GetBufferIndexForALayout(LayoutCarrier& a_valid_layout) noexcept
    {
        if (!ValidateALayoutCarrier(a_valid_layout, true))
        {
            return std::nullopt;
        }

        const uint8_t start_idx = static_cast<uint8_t>(APCPagedNodeSegmentClasses::FEEDFORWARD_MESSAGE);
        return static_cast<uint8_t>(static_cast<uint8_t>(a_valid_layout.LayoutIdentity) - start_idx);
    }

    static constexpr bool InsertALayoutCellInBuffer(
        LayoutBufferOfAPC& layout_buffer,
        LayoutCarrier& a_valid_layout_carrier
    ) noexcept
    {
        std::optional<uint8_t> maybe_buffer_idx = GetBufferIndexForALayout(a_valid_layout_carrier);
        if (!maybe_buffer_idx.has_value())
        {
            return false;
        }

        const packed64_t layout_cell = CreateALayoutBoundsCell(a_valid_layout_carrier);

        if (layout_cell == PackedCell64_t::PACKED_CELL_SENTINAL)
        {
            return false;
        }

        layout_buffer[maybe_buffer_idx.value()] = layout_cell;
        
        return true;
    }


    // static constexpr bool BuildInitialLayoutBuffer(
    //     LayoutBufferOfAPC& return_buffer,
    //     uint16_t capacity_of_the_apc,
    //     uint8_t desired_version = APCDataStructure::BRANCH_VERSION
    // ) noexcept
    // {
    //     BuildNullLayoutBuffer(return_buffer);
    //     if (
    //         capacity_of_the_apc < MINIMUM_BRANCH_CAPACITY ||
    //         !APCDataStructure::IsThisIndexValidForAPC(capacity_of_the_apc) ||
    //         !APCDataStructure::ThisVersionValid(desired_version)
    //     )
    //     {
    //         return false;
    //     }

    //     const uint16_t payload_begin = static_cast<uint16_t>(APCDataStructure::METACELL_COUNT);
    //     const uint16_t payload_span = capacity_of_the_apc - payload_begin;




        
    // }

    // static constexpr bool ValidateALayoutBuffer(
    //     LayoutBufferOfAPC& return_buffer,
    //     uint32_t capacity_of_the_apc
    // ) noexcept
    // {

    // }

    static constexpr bool MutateAPCLayout() noexcept;

};
}