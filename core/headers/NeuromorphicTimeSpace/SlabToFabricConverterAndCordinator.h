#pragma once 
#include "FabricTableConstructors/CompleteFabric.h"

namespace BidirectionalInMemGraph
{
    
    
    class SlabToFabricConverterAndCordinator : public EdgeTableConstructor
    {
    private:

        uint64_t* AllocatePackedCellRaw_(size_t count_of_cells) noexcept;
        
        void FreeRawPackedCells_(uint64_t*packed_cell_memory_ptr, size_t packed_cell_count) noexcept;

        void ResetScalarsofTheFabric_() noexcept;

        /// @brief INITIALIZES: All FabricMetaIndicies
        /// @param table_directory_begin 
        /// @param table_directory_end 
        void InitializeCompleateFabricMetaIndices_(size_t record_book_begin, size_t record_book_end) noexcept;

    protected :
        bool AttachValidIdentity(uint32_t apc_idx) noexcept;

        bool InitializeFabric(
            uint32_t slot_count,
            uint32_t slot_cell_count = MINIMUM_APC_CELL_COUNT
        ) noexcept;
    public:
        SlabToFabricConverterAndCordinator(/* args */) noexcept = default;

        ~SlabToFabricConverterAndCordinator() noexcept
        {
            ShutDownFabric();
        }

        SlabToFabricConverterAndCordinator(const SlabToFabricConverterAndCordinator&) = delete;
        SlabToFabricConverterAndCordinator& operator = (const SlabToFabricConverterAndCordinator&) = delete;

        void ShutDownFabric() noexcept;

        bool IsFabricActive() noexcept
        {
            return
                FabricInitialized_.load(std::memory_order_acquire) &&
                SlabBasePtr_ &&
                APCDataStructure::IsValid32BitAPCUnit(PerAPCRuntimeCellCount_) &&
                APCDataStructure::IsValid32BitAPCUnit(CountOfAPC_);
        }
        
    };

    class ForestMutationConf : public SlabToFabricConverterAndCordinator
    {
    protected:

        static constexpr uint8_t FOREST_MAX_EDGE_PERTICIPENT_ = 5u;
        static constexpr uint8_t FOREST_MAX_APC_PERTICIPENT_ = 4u;

        using AllRequiresApcList_ = std::array<uint32_t, FOREST_MAX_APC_PERTICIPENT_>;

        void WriteAcquiredAxisDelta_(
            uint32_t apc_slot,
            const IAB::BufferOfAPCIdentity& before_idintity,
            const IAB::BufferOfAPCIdentity& desired_identity,
            IAB::BidirectionalAxis axis
        ) noexcept;

        bool ReleseGraphMutationFlag_(
            uint32_t apc_slot,
            IAB::BidirectionalAxis axis,
            uint32_t max_tries = DEFAULT_MAX_TRIES
        ) noexcept;

        struct ForestEdgePerticipent_
        {
            uint32_t Index = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
            EdgeBuilder::EdgeData Before{};
            EdgeBuilder::EdgeData Work{};
            EdgeBuilder::EdgeStatus ExpectedStatus = EdgeBuilder::EdgeStatus::LIVE;

            bool IsLocalParticipent = true;
            bool IsForestGate = false;

            bool Reserved = false;
            bool Published = false;
        };

        struct ForestAPCPerticipent_
        {
            uint32_t Slot = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
            IAB::BufferOfAPCIdentity Before{};
            IAB::BufferOfAPCIdentity Work{};
            bool Locked = false;
            bool Published = false;
        };

        struct ForestMutationTransaction_
        {
            IAB::BidirectionalAxis Axis{};
            std::array<ForestEdgePerticipent_, FOREST_MAX_EDGE_PERTICIPENT_> Edges{};
            std::array<ForestAPCPerticipent_, FOREST_MAX_APC_PERTICIPENT_> Identities{};

            uint8_t EdgeCount = UNSIGNED_ZERO;
            uint8_t APCCount = UNSIGNED_ZERO;

            bool AllGraphRelesed = true;
        };

        bool AddForestEdgeParticipent_(
            ForestMutationTransaction_& trasaction,
            uint32_t edge_idx,
            bool is_local_perticipent = true,
            bool is_forest_gate = false,
            EdgeBuilder::EdgeStatus expacted_state = EdgeBuilder::EdgeStatus::LIVE
        ) noexcept;

        ForestEdgePerticipent_* FindForestEdgeParticipent_(
            ForestMutationTransaction_& transaction,
            uint32_t edge_idx 
        ) noexcept;

        ForestAPCPerticipent_* FindForestAPCParticipent_(
            ForestMutationTransaction_& transaction,
            uint32_t apc_slot 
        ) noexcept;

        void RestoreForestEdges_(
            ForestMutationTransaction_& transaction,
            uint32_t max_tries = DEFAULT_MAX_TRIES
        ) noexcept;

        void RestoreForestIdentities_(
            ForestMutationTransaction_& transaction
        ) noexcept;

        void ReleseAxisReservation_(
            ForestMutationTransaction_& transaction,
            uint32_t max_tries = DEFAULT_MAX_TRIES
        ) noexcept;

        void AbroatForestMutation_(
            ForestMutationTransaction_ treansaction,
            uint32_t max_tries = DEFAULT_MAX_TRIES
        ) noexcept;

        bool ReserveLocalForestEdges_(
            ForestMutationTransaction_& transaction,
            uint32_t max_tries = DEFAULT_MAX_TRIES
        ) noexcept;

        bool CommitForestMutation_(
            ForestMutationTransaction_& transaction,
            uint32_t max_tries = DEFAULT_MAX_TRIES
        ) noexcept;

        SeqLockedOperation ReadCommittedForestEdge_(
            uint32_t edge_idx,
            IAB::BidirectionalAxis axis,
            EdgeBuilder::EdgeData& edge_data,
            ForestMutationTransaction_* transaction = nullptr
        ) noexcept;
    };

    class ConstructForestOnEachAxis : public ForestMutationConf
    {
        friend class AdaptivePackedCellContainer;
        friend class FabricToAPCLinker;
    private:

        bool AcquiteAllIdentitiesForTransaction_(
            AllRequiresApcList_& slots,
            uint8_t slot_count,
            IAB::BidirectionalAxis axis,
            ForestMutationTransaction_& transaction,
            uint32_t internal_max_tries = DEFAULT_MAX_TRIES
        ) noexcept;

        SeqLockedOperation FindForestRootEdge_(
            uint32_t start_edge_idx,
            uint32_t forbidden_edge_idx,
            bool reject_forbidden,
            uint32_t& root_edge_idx,
            IAB::BidirectionalAxis axis,
            ForestMutationTransaction_& transaction
        ) noexcept;

        bool AnchorADetachedChildToParent(
            uint32_t predessor_idx,
            uint32_t child_idx,
            IAB::BidirectionalAxis axis,
            IAB::DescOfInharitance inharitance,
            uint32_t internal_max_tries = DEFAULT_MAX_TRIES
        ) noexcept;

        bool UnlinkTwoAPC(
            uint32_t child_idx,
            IAB::BidirectionalAxis axis,
            uint32_t internal_max_tries = DEFAULT_MAX_TRIES
        ) noexcept;

        /// @return PREVIOUS GRAPH MUTATION VALUE RAW: MEANS: Value before change
        std::optional<uint64_t> AcquireGraphMutationFlag_(
            uint32_t apc_slot_idx,
            IAB::BidirectionalAxis axis,
            uint32_t max_tries = DEFAULT_MAX_TRIES
        ) noexcept;

        bool UnlinkAndRelinkToTail(
            uint32_t apc_slot_idx,
            uint32_t unlink_edge_idx,
            uint32_t relink_edge_idx,
            IAB::BidirectionalAxis axis,
            uint32_t internal_max_tries = DEFAULT_MAX_TRIES
        ) noexcept;

        bool RetireAPC_(
            uint32_t slot, 
            uint32_t generation,
            uint32_t max_tries = DEFAULT_MAX_TRIES
        ) noexcept;
        
    protected:
        SeqLockedOperation ReadIdentityBufferOfAPC(
            uint32_t apc_slot,
            IAB::BufferOfAPCIdentity& identity,
            std::optional<IAB::BidirectionalAxis> axis = std::nullopt,
            bool is_axis_already_reserved = false
        ) noexcept;

        bool OpenForestGateOnAxis(
            uint32_t apc_slot,
            IAB::BidirectionalAxis axis,
            uint32_t max_tries = DEFAULT_MAX_TRIES
        ) noexcept;


        bool ReclaimRetiredSlot_(uint32_t slot) noexcept;

    public:
        // Just for test
        bool ReadGraphMutationFlags(
            uint32_t slot_idx,
            IAB::GraphMutationValues& values
        ) noexcept;

};

}