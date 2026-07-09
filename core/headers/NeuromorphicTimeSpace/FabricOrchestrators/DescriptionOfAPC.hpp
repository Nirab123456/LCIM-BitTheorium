#pragma once 
#include "CoreOfFabricCoordinator.hpp"

namespace PredictedAdaptedEncoding
{

struct APCDescriptorRange
{
    size_t BeginIndex = UNSIGNED_ZERO;
    size_t EndIndex = UNSIGNED_ZERO;
    bool IsValid = false;
};
static_assert(sizeof(APCDescriptorRange) == RECORD_BOOK_WIDTH * sizeof(packed64_t));
static_assert(alignof(APCDescriptorRange) == alignof(packed64_t));


struct DescriptionOfAPC
{

    static constexpr uint64_t VALID_BUFFER_MARK = 1111111111111;

    using SingleAPCDescriptionCellBuffer = std::array<packed64_t, APC_DESCRIPTOR_WIDTH_OR_VALIDATION_INDEX + 1>;

    /// @brief Assignes UINT64_MAX  UPTO:INDEX: APC_DESCRIPTOR_WIDTH_OR_VALIDATION_INDEX - 1 and Next 2 INDEX: UNSIGNED_ZERO
    /// @param default_array 
    static constexpr void BuildABlankAPCDescriptionBufferwith2CellIdentity(SingleAPCDescriptionCellBuffer& default_array)
    {
        for (size_t i = 0; i < default_array.size(); i++)
        {
            if (i < APC_DESCRIPTOR_WIDTH_OR_VALIDATION_INDEX)
            {
                default_array[i] = PackedCell64_t::PACKED_CELL_SENTINAL;
            }
            else
            {
                default_array[i] = UNSIGNED_ZERO;
            }
        }
    }

    enum class StateOfSingleAPCDescription : uint8_t
    {
        EMPTY_RECORD = 0,
        RECORD_WITH_SEGMENT_POOL = 1,
        OWNED_BY_APC = 2,
        RETIREMENT_REQUEST = 3,
        LOGICALY_RETIRED = 4,
        UNASSIGNED_UNUSED_NANNULL = 5
    };

    static constexpr bool IsValidValue48APCDescription(APCDescriptorCellType descriptor_cell_type) noexcept
    {
        if (descriptor_cell_type < APCDescriptorCellType::OCCUPANCY_CELL16x3)
        {
            return true;
        }
        return false;
    }

    static constexpr bool IsValidAPCDescriptionCell(
        packed64_t packed_cell, 
        std::optional<PackedMode> mode_check = std::nullopt,
        bool check_consumeablity = true,
        PackedCell64_t::AuthoritiveCellView* return_auth_view_ptr = nullptr
    ) noexcept
    {
        PackedCell64_t::AuthoritiveCellView desired_auth_view = PackedCell64_t::GetAuthoritiveViewsForACell(packed_cell);

        if (return_auth_view_ptr)
        {
            *return_auth_view_ptr = desired_auth_view;
        }

        if (!CoreOfFabricCoordinator::CommonValidityCheckOfFabricCellsTableSegmentClasses(desired_auth_view) ||
            desired_auth_view.FabricTableSegmentClass != FabricTableSegmentClasses::APC_HANDLE_DESCRIPTOR
        )
        {
            return false;
        }

        if ( 
            check_consumeablity && 
            desired_auth_view.LocalityOfCell == LocalityPolicy::CLAIMED
        )
        {
            return false;
        }

        if (mode_check.has_value() && mode_check != desired_auth_view.CellMode)
        {
            return false;
        }
        

        switch (desired_auth_view.CellMode)
        {

        case PackedMode::VALUE48:
            return desired_auth_view.ContractOfValue == ContractOfConcurrency::CLAIMED_GURDED;
        
        case PackedMode::MODEL48:
            return desired_auth_view.SubClassOfModel48 == Model48Subclass::SUBDIVISION16x3_INTERNAL_CELL_MODEL;

        case PackedMode::MODEL32:
            return desired_auth_view.SubClassOfModel32 == Model32Subclass::UNCLOCKED_1x8_PLUS_2x4;
        
        default:
            return false;
        }
        
    }


    static constexpr packed64_t MakeAPCDescriptionValue48Cell(
        uint64_t cell_value, 
        LocalityPolicy locality = LocalityPolicy::PUBLISHED
    ) noexcept
    {
        return PackedCell64_t::MakeTypedFabricValidPackedCell(
            TypeFamily::VALUE48,
            ContractOfConcurrency::CLAIMED_GURDED,
            FabricTableSegmentClasses::APC_HANDLE_DESCRIPTOR,
            locality,
            InternalDataTypePolicy ::UNSIGNED,
            WildCardOfPackedCell::PACKED_CELL,
            cell_value
        );
        
    }


    /// @brief USES: Model48Subclass::SUBDIVISION16x3_INTERNAL_CELL_MODEL & CREATS: Occupancy cell OwnershipPolicy::NEUROMORPHIC_SPACE_TIME_FABRIC  [LocalityPolicy::PUBLISHED | LocalityPolicy::CLAIMED | LocalityPolicy::FAULTY]
    /// @param published_count LOWEST: uint16_t in PackUnsigned16x3ToMode48_
    /// @param claimed_count MID: uint16_t in PackUnsigned16x3ToMode48_
    /// @param faulty_count HIGHIEST: uint16_t in PackUnsigned16x3ToMode48_
    /// @return 
    static constexpr packed64_t MakeOccupancyDescriptionForAPCDescriptionTable(
        uint16_t published_count,
        uint16_t claimed_count,
        uint16_t faulty_count,
        LocalityPolicy locality = LocalityPolicy::PUBLISHED
    ) noexcept
    {   
        const uint64_t raw_48 =  Subdevision16x3InternalMode48CellModel::PackUnsigned16x3ToMode48_(
            published_count,
            claimed_count,
            faulty_count
        );

        return PackedCell64_t::MakeModeledFabricValidPackedCell(
            ModelFamily::MODEL48,
            static_cast<tag8_t>(Model48Subclass::SUBDIVISION16x3_INTERNAL_CELL_MODEL),
            FabricTableSegmentClasses::APC_HANDLE_DESCRIPTOR,
            locality,
            InternalDataTypePolicy ::UNSIGNED,
            WildCardOfPackedCell::PACKED_CELL,
            raw_48
        );
    }

    /// @brief 
    /// @param state_of_apc MUST PACKEDMODE:MODEL32 -> AND Model32Subclass::UNCLOCKED_1x8_PLUS_2x4 -> [LOWEST16-> VERSION | MID16 -> State Of APC | HIGHIEST16 -> Ownership Of APC] 
    /// @return CELL INVALID: std::nullopt / HashFilesCarrier with Validity flag
    static constexpr packed64_t MakeStateandSaftyCellOfSingleAPCDescriptor(
        size_t segment_pool_begin, 
        size_t segment_pool_end,
        StateOfSingleAPCDescription state_of_apc,
        uint8_t version,
        LocalityPolicy locality = LocalityPolicy::PUBLISHED,
        OwnershipPolicy origin_ownership = OwnershipPolicy::NEUROMORPHIC_SPACE_TIME_FABRIC
    )
    {

        const uint32_t apc_width = static_cast<uint32_t>(segment_pool_end - segment_pool_begin);

        uint16_t state_version_ownership = UNSIGNED_ZERO;
        if (apc_width == UNSIGNED_ZERO || !APCDataStructure::IsThisIndexValidForAPC(apc_width))
        {
            return PackedCell64_t::PACKED_CELL_SENTINAL;
        }
        
        state_version_ownership = Clock16Subdivision1x8Plus2x4InMode32CellModel::Pack1x8Plus2x4InUnsigned16_(version, static_cast<uint8_t>(state_of_apc), static_cast<uint8_t>(origin_ownership));

        return PackedCell64_t::MakeModeledFabricValidPackedCell(
            ModelFamily::MODEL32,
            static_cast<tag8_t>(Model32Subclass::UNCLOCKED_1x8_PLUS_2x4),
            FabricTableSegmentClasses::APC_HANDLE_DESCRIPTOR,
            locality, InternalDataTypePolicy::UNSIGNED,
            WildCardOfPackedCell::PACKED_CELL,
            apc_width,
            state_version_ownership
        );

    }

    /// @brief Sets Desired State, Owner & Version inside Safty Cell -> ONY: Bare Matale Packed Cell Check
    /// @return WARNING: This Function Dosent Validate ANYTHING & User Defined Value May Broke The Description If Not Validated OR: Carefull Use
    static constexpr packed64_t SwitchStateOrAPCOwnerOfSaftyCell(
        packed64_t packed_cell,
        StateOfSingleAPCDescription desired_state = StateOfSingleAPCDescription::UNASSIGNED_UNUSED_NANNULL,
        OwnershipPolicy desired_owner_of_apc = OwnershipPolicy::UNASSIGNED_UNUSED_NANNULL,
        uint8_t desired_version = UNSIGNED_ZERO
    ) noexcept
    {
        const DescriptorSaftyFiles current_safty_files = ReadFilesFromStateSaftyofADescriptor(packed_cell);
        if (current_safty_files.IsValid)
        {
            return PackedCell64_t::PACKED_CELL_SENTINAL;
        }
        
        const StateOfSingleAPCDescription state_value = desired_state == StateOfSingleAPCDescription::UNASSIGNED_UNUSED_NANNULL ? current_safty_files.StateOfTheAPC : desired_state;
        const OwnershipPolicy owner = desired_owner_of_apc == OwnershipPolicy::UNASSIGNED_UNUSED_NANNULL ? current_safty_files.WhoHoldsTheAcess : desired_owner_of_apc;
        const uint8_t version = desired_version == UNSIGNED_ZERO ? current_safty_files.Version : desired_version;

        const uint16_t state_version_ownership = Clock16Subdivision1x8Plus2x4InMode32CellModel::Pack1x8Plus2x4InUnsigned16_(
            version, 
            static_cast<uint8_t>(state_value), 
            static_cast<uint8_t>(owner)
        );

        return PackedCell64_t::SetMETA16InPacked(packed_cell, state_version_ownership);
    }

    struct DescriptorSaftyFiles
    {
        uint16_t WidthOfAPC = APCDataStructure::APC_INDEX_SENTINAL;
        uint8_t Version = UINT8_MAX;
        StateOfSingleAPCDescription StateOfTheAPC = StateOfSingleAPCDescription::UNASSIGNED_UNUSED_NANNULL;
        OwnershipPolicy WhoHoldsTheAcess = OwnershipPolicy::UNASSIGNED_UNUSED_NANNULL;
        LocalityPolicy LocalityOfTheDescription = LocalityPolicy::UNASSIGNED_UNUSED_NANNULL;
        bool IsValid = false;
    };
    static_assert(sizeof(DescriptorSaftyFiles) <= sizeof(packed64_t));

    /// @brief Returns A Compleate View of the APCDescriptorCellType::STATE_OWNERSHIP_VESION_SAFTY
    /// @param packed_cell 
    /// @return 
    static constexpr DescriptorSaftyFiles ReadFilesFromStateSaftyofADescriptor(packed64_t packed_cell) noexcept
    {
        DescriptorSaftyFiles return_descriptor_files{};

        PackedCell64_t::AuthoritiveCellView desired_cell_view{};

        if (!IsValidAPCDescriptionCell(packed_cell, PackedMode::MODEL32, false, &desired_cell_view))
        {
            return return_descriptor_files;
        }

        return_descriptor_files.WidthOfAPC = static_cast<uint16_t>(desired_cell_view.Raw32BitInCellData);

        return_descriptor_files.Version = Clock16Subdivision1x8Plus2x4InMode32CellModel::ExtractLowest8Bit_(desired_cell_view.InCellClock16);

        return_descriptor_files.StateOfTheAPC = static_cast<StateOfSingleAPCDescription>(
            Clock16Subdivision1x8Plus2x4InMode32CellModel::ExtractMid4Bit_(desired_cell_view.InCellClock16)
        );

        return_descriptor_files.WhoHoldsTheAcess = static_cast<OwnershipPolicy>(
            Clock16Subdivision1x8Plus2x4InMode32CellModel::ExtractHighiest4Bit_(desired_cell_view.InCellClock16)
        );

        return_descriptor_files.LocalityOfTheDescription = desired_cell_view.LocalityOfCell;

        if (
            return_descriptor_files.WidthOfAPC >= MINIMUM_APC_CAPACITY  && 
            APCDataStructure::IsThisIndexValidForAPC(return_descriptor_files.WidthOfAPC) &&
            return_descriptor_files.StateOfTheAPC != StateOfSingleAPCDescription::UNASSIGNED_UNUSED_NANNULL &&
            return_descriptor_files.WhoHoldsTheAcess != OwnershipPolicy::UNASSIGNED_UNUSED_NANNULL
        )
        {
            return_descriptor_files.IsValid = true;
            return return_descriptor_files;
        }

        return return_descriptor_files;
    }

    /// @brief Matches The SingleAPCDescriptionCellBuffer's -> RECORDED: Width vs STORED: Files IN: APCDescriptorCellType::STATE_OWNERSHIP_VESION_SAFTY
    /// @param single_apc_description 
    /// @param validate_observer Who Ownes the APC Currently & If set And validate_observer != Current Owner -> Will RETURN: false
    /// @param desired_state Whats the CURRENT: StateOfSingleAPCDescription value & IF: Set desired_state != StateOfSingleAPCDescription -> Will RETURN: false
    /// @param version_match IF: Set snd VERSION: Dosent match the current version -> Will RETURN: false
    static constexpr bool ValidateDescriptionBufferBySaftyFiles(
        SingleAPCDescriptionCellBuffer& single_apc_description,
        OwnershipPolicy validate_observer = OwnershipPolicy::UNASSIGNED_UNUSED_NANNULL,
        StateOfSingleAPCDescription desired_state = StateOfSingleAPCDescription::UNASSIGNED_UNUSED_NANNULL,
        std::optional<uint8_t> version_match = std::nullopt
    ) noexcept
    {
        const packed64_t segment_pool_begin_cell =  single_apc_description[static_cast<size_t>(APCDescriptorCellType::APC_SEGMENTPOOL_BEGAIN_SLAB)];
        const packed64_t segment_pool_end_cell = single_apc_description[static_cast<size_t>(APCDescriptorCellType::APC_SEGMENTPOOL_END_SLAB)];
        const packed64_t state_version_ownership_cell = single_apc_description[static_cast<size_t>(APCDescriptorCellType::STATE_OWNERSHIP_VESION_SAFTY)];

        if (
            !IsValidAPCDescriptionCell(segment_pool_begin_cell, PackedMode::VALUE48, true) ||
            !IsValidAPCDescriptionCell(segment_pool_end_cell, PackedMode::VALUE48, true) ||
            !IsValidAPCDescriptionCell(state_version_ownership_cell, PackedMode::MODEL32, true)
        )
        {
            return false;
        }
        
        const PackedCell64_t::AuthoritiveCellView auth_view_of_state_version_ownership = PackedCell64_t::GetAuthoritiveViewsForACell(state_version_ownership_cell);

        const uint64_t segment_pool_begin = PackedCell64_t::ExtractRaw48FamilyBits(segment_pool_begin_cell);
        const uint64_t segment_pool_end = PackedCell64_t::ExtractRaw48FamilyBits(segment_pool_end_cell);


        const uint32_t apc_width = static_cast<uint32_t>(segment_pool_end - segment_pool_begin);


        if (apc_width == UNSIGNED_ZERO || auth_view_of_state_version_ownership.Raw32BitInCellData != apc_width)
        {
            return false;
        }

        const uint8_t version = Clock16Subdivision1x8Plus2x4InMode32CellModel::ExtractLowest8Bit_(auth_view_of_state_version_ownership.InCellClock16);
        const StateOfSingleAPCDescription state_of_description = static_cast<StateOfSingleAPCDescription>(Clock16Subdivision1x8Plus2x4InMode32CellModel::ExtractMid4Bit_(auth_view_of_state_version_ownership.InCellClock16));
        const OwnershipPolicy ownershipe_of_description = static_cast<OwnershipPolicy>(Clock16Subdivision1x8Plus2x4InMode32CellModel::ExtractHighiest4Bit_(auth_view_of_state_version_ownership.InCellClock16));
        
        if (validate_observer != OwnershipPolicy::UNASSIGNED_UNUSED_NANNULL && validate_observer != ownershipe_of_description)
        {
            return false;
        }

        if (desired_state != StateOfSingleAPCDescription::UNASSIGNED_UNUSED_NANNULL && desired_state != state_of_description)
        {
            return false;
        }

        if (version_match.has_value() && *version_match != version)
        {
            return false;
        }
        
        return true;

    }

    static constexpr void SetADescriptionInCellBuffer(
        SingleAPCDescriptionCellBuffer& single_apc_description_buffer,
        APCDescriptorCellType cell_type, 
        uint64_t cell_value,
        LocalityPolicy cell_locality = LocalityPolicy::PUBLISHED
    ) noexcept
    {
        if (!IsValidValue48APCDescription(cell_type))
        {
            return;
        }
        
        const packed64_t desired_packed_cell = MakeAPCDescriptionValue48Cell(cell_value, cell_locality);

        single_apc_description_buffer[static_cast<size_t>(cell_type)] = desired_packed_cell;
    }  
    
    /// @brief USES: Model48Subclass::SUBDIVISION16x3_INTERNAL_CELL_MODEL & CREATES: Occupancy cell OwnershipPolicy::NEUROMORPHIC_SPACE_TIME_FABRIC [LocalityPolicy::PUBLISHED | LocalityPolicy::CLAIMED | LocalityPolicy::FAULTY]
    /// @param single_apc_description_buffer 
    /// @param published_count 
    /// @param claimed_count 
    /// @param faulty_count 
    /// @param locality 
    static constexpr void SetOccupancyInBuffer(
        SingleAPCDescriptionCellBuffer& single_apc_description_buffer,
        uint16_t published_count,
        uint16_t claimed_count,
        uint16_t faulty_count,
        LocalityPolicy locality = LocalityPolicy::PUBLISHED
    )
    {
        const packed64_t desired_occupancy_cell = MakeOccupancyDescriptionForAPCDescriptionTable(
            published_count,
            claimed_count,
            faulty_count,
            locality
        );

        single_apc_description_buffer[static_cast<size_t>(APCDescriptorCellType::OCCUPANCY_CELL16x3)] = desired_occupancy_cell;
    }

    static constexpr bool SetStateSaftyCellInBuffer(SingleAPCDescriptionCellBuffer& a_buffer_description, packed64_t packed_cell) noexcept
    {
        if (!PackedCell64_t::IsThisCellValid(packed_cell))
        {
            return false;
        }

        a_buffer_description[static_cast<size_t>(APCDescriptorCellType::STATE_OWNERSHIP_VESION_SAFTY)] = packed_cell;
        return true;
    }


    static constexpr std::optional<uint64_t> ReadADataValueFromADescriptionBuffer(
        SingleAPCDescriptionCellBuffer& a_description_buffer, 
        APCDescriptorCellType description_type, 
        bool check_consumeablity = true
    ) noexcept
    {
        if (!IsValidValue48APCDescription(description_type))
        {
            return std::nullopt;
        }

        if (!ValidateSingleAPCDescriptionBuffer(a_description_buffer, check_consumeablity))
        {
            return std::nullopt;
        }

        const packed64_t desired_value = PackedCell64_t::ExtractRaw48FamilyBits(a_description_buffer[static_cast<size_t>(description_type)]);

        if (desired_value != PackedCell64_t::PACKED_CELL_SENTINAL)
        {
            return desired_value;
        }

        return std::nullopt;
    }


    /// @brief VALIDATES: INDEX 0 <-> (single_apc_description_buffer.size() - 2) if valid sets VALID_BUFFER_MARK to index (APC_DESCRIPTOR_WIDTH_OR_VALIDATION_INDEX):: Means valid
    /// @param single_apc_description_buffer ADDRESS: OF: SingleAPCDescriptionCellBuffer to check
    /// @param check_consumeablity MEANS: IF true -> LocalityPolicy::CLAIMED ->INVALID
    /// @param validate_observer Fabric can observe when ownership to APC but APC cant observe when ownership to fabric 
    /// @param desired_state  APC shouldnt observe StateOfSingleAPCDescription::EMPTY_RECORD / StateOfSingleAPCDescription::LOGICALY_RETIRED
    /// @param version_match VERSION:: Probably optional for now 
    /// @return 
    static constexpr bool ValidateSingleAPCDescriptionBuffer(
        SingleAPCDescriptionCellBuffer& single_apc_description_buffer,
        bool check_consumeablity = true,
        OwnershipPolicy validate_observer = OwnershipPolicy::UNASSIGNED_UNUSED_NANNULL,
        StateOfSingleAPCDescription desired_state = StateOfSingleAPCDescription::UNASSIGNED_UNUSED_NANNULL,
        uint64_t match_apc_idx_itself = PackedCell64_t::PACKED_CELL_SENTINAL,
        std::optional<uint8_t> version_match = std::nullopt
    ) noexcept
    {
        for (size_t i = 0; i < static_cast<size_t>(APCDescriptorCellType::OCCUPANCY_CELL16x3); i++)
        {
            if (!IsValidAPCDescriptionCell(single_apc_description_buffer[i], PackedMode::VALUE48, check_consumeablity))
            {
                return false;
            }            
        }

        if (
            !IsValidAPCDescriptionCell(
                single_apc_description_buffer[static_cast<size_t>(APCDescriptorCellType::OCCUPANCY_CELL16x3)], 
                PackedMode::MODEL48, 
                check_consumeablity
            ) ||
            !ValidateDescriptionBufferBySaftyFiles(single_apc_description_buffer, validate_observer, desired_state, version_match) 
        )
        {
            return false;
        }
        if (match_apc_idx_itself <= PackedCell64_t::BIT_FAMILY_48_SENTINAL)
        {
            const uint64_t saved_idx_in_buffer = PackedCell64_t::ExtractRaw48FamilyBits(
                single_apc_description_buffer[static_cast<size_t>(APCDescriptorCellType::CURRENT_DESCRIPTOR_INDEX)]
            );
            if (match_apc_idx_itself != saved_idx_in_buffer)
            {
                return false;
            }
        }

        single_apc_description_buffer[APC_DESCRIPTOR_WIDTH_OR_VALIDATION_INDEX] = VALID_BUFFER_MARK;
        
        return true;
    }


    /// @brief MAKES: VALIDATED DEFAULT: DESCRIPTION USING StateOfSingleAPCDescription::RECORD_WITH_SEGMENT_POOL & OwnershipPolicy::NEUROMORPHIC_SPACE_TIME_FABRIC
    /// @return UNVALID: SingleAPCDescriptionCellBuffer -> LAST INDEX: 0 UNSIGNED_ZERO | Please use ValidateSingleAPCDescriptionBuffer ON: RETURN:SingleAPCDescriptionCellBuffer before storing
    static constexpr SingleAPCDescriptionCellBuffer MakeADefaultAPCDescription(
        uint64_t sequential_index_current_description,
        uint64_t segment_pool_begin,
        uint64_t segment_pool_end,
        uint64_t segment_pool_begin_for_next_apc = UNSIGNED_ZERO,
        uint8_t version = UNSIGNED_ZERO,
        LocalityPolicy cell_locality = LocalityPolicy::PUBLISHED
    ) noexcept
    {
        SingleAPCDescriptionCellBuffer apc_description_buffer{};

        SetADescriptionInCellBuffer(apc_description_buffer, APCDescriptorCellType::CURRENT_DESCRIPTOR_INDEX, sequential_index_current_description, cell_locality);
        SetADescriptionInCellBuffer(apc_description_buffer, APCDescriptorCellType::OWNER_BRANCH, UNSIGNED_ZERO, cell_locality);
        SetADescriptionInCellBuffer(apc_description_buffer, APCDescriptorCellType::APC_SEGMENTPOOL_BEGAIN_SLAB, segment_pool_begin, cell_locality);
        SetADescriptionInCellBuffer(apc_description_buffer, APCDescriptorCellType::APC_SEGMENTPOOL_END_SLAB, segment_pool_end, cell_locality);
        SetADescriptionInCellBuffer(apc_description_buffer, APCDescriptorCellType::NEXT_APC_SAGMANTPOOL_BEGAIN, segment_pool_begin_for_next_apc, cell_locality);
        SetADescriptionInCellBuffer(apc_description_buffer, APCDescriptorCellType::LOGICAL_ID, UNSIGNED_ZERO, cell_locality);
        SetADescriptionInCellBuffer(apc_description_buffer, APCDescriptorCellType::SHARED_GROUP_ID, UNSIGNED_ZERO, cell_locality);
        SetADescriptionInCellBuffer(apc_description_buffer, APCDescriptorCellType::RELATION_HEADS, UNSIGNED_ZERO, cell_locality);
        SetADescriptionInCellBuffer(apc_description_buffer, APCDescriptorCellType::RETIRE_EPOCH48, UNSIGNED_ZERO, cell_locality);
        SetADescriptionInCellBuffer(apc_description_buffer, APCDescriptorCellType::APC_FLAGS_FOR_THIS, UNSIGNED_ZERO, cell_locality);
        SetOccupancyInBuffer(apc_description_buffer, UNSIGNED_ZERO, UNSIGNED_ZERO, UNSIGNED_ZERO, cell_locality);
        const packed64_t state_ownership_version_cell =  MakeStateandSaftyCellOfSingleAPCDescriptor(
            segment_pool_begin, segment_pool_end, StateOfSingleAPCDescription::RECORD_WITH_SEGMENT_POOL, 
            version, cell_locality, OwnershipPolicy::NEUROMORPHIC_SPACE_TIME_FABRIC
        );

        apc_description_buffer[static_cast<size_t>(APCDescriptorCellType::STATE_OWNERSHIP_VESION_SAFTY)] = state_ownership_version_cell;

        apc_description_buffer[APC_DESCRIPTOR_WIDTH_OR_VALIDATION_INDEX] = UNSIGNED_ZERO;

        return apc_description_buffer;
    }


};


}