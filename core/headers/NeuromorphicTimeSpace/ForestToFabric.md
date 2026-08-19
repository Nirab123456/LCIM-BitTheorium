1. RangeOfAPC APCLifeCycle::GetSegmentPoolRange :: Reads Range of A APC statically not from descriptor the whole architecture is built on this, still keeping track of APC range inside FabricSegments::APC_HANDLE_DESCRIPTOR

2. APCLifeCycle::ReadAPCStateAtomically_ : Reads state of the APC 

3. bool ConstructAPCIdentity::ReleseGraphMutationFlag_ : Reads APC range by -> GetSegmentPoolRange : checks if the axis available to relese if so releses

4. ConstructAPCIdentity::WriteAcquiredAxisDelta_ : Compares current and previous locally and updates only the changed field of desired axis and it is unsafe if miseused.

6. EdgeTableConstructor::PublishReservedEdge_ : Checks current edge status = EdgeStatus::RESERVED build a valid Edge buffer with with updated sequense lock and provided data by EdgeBuilder::EdgeData struct and just updates atomically.

5. ConstructAPCIdentity::InitiateRootAxis : 
    (Check EDGE == RESERVED) -> (DESCRIPTION == LIVE) -> (Call To WriteAcquiredAxisDelta_) -> (Call To PublishReservedEdge_)
    FAILURE IN ANY POINT :
    (ReleseGraphMutationFlag_) -> (RELESE: Axis lock by PublishReservedEdge_)

6. GetMemGFlagFromAxis : Returns the enum flug which should be truned ON/OFF based on the provided axis.

7. FabricConstructor::ReadASnapShotFromSlab : Reades a range in slab to provided 
    buffer it is just an memcpy.

8. InstallAxisToBuffer:ValidateIdentityBuffer : ValidateDefaultIdentity + IsValidGraphMutationState.

9. std::optional<StateOfAPC> ConstructAPCIdentity::ReadIdentityBufferOfAPC : 
    I.GetSegmentPoolRange + ReadAPCStateAtomically_ :: Failure in either returns std::nullopt means read operation itself is invalid,
    II. ReadASnapShotFromSlab -> with APCDataStructure::TotalIdentityUnitCount() + ValidateIdentityBuffer :: If either
    fails returns std::nullopt
    OVERVIEW:
        if return is std::nullopt it dosent means structure has failure it simply means it cant verify enough relation to read idintity buffer. Though the function allows read of the idintity buffer regardless of the state of apc thats the reason why state of apc is returened insted of simple boolean.

10. EdgeTableConstructor::SwitchEdgeState__ : Checks if the switch request valid in contrast to current status of the Edge table and sequense Lock.
    EXTENSIONS:
        I. ReserveAnEdge_

11. ConstructAPCIdentity::AcquireGraphMutationFlag_ : GetSegmentPoolRange +
    ReadAPCStateAtomically_ : varifies the desired mutation flag In contrast to current mutation flags if valid acquires the flag.

12. PrepareInharitedAxis : Prepares both identity + Both Edge Table Data EdgeData (owner edge which is bing muted and if 
    the current APC who is bing linked and unlinked to a owner is also a owner then prepers that edge too)

13. ConstructAPCIdentity::LinkTwoAPC : 
    ReadIdentityBufferOfAPC -> ReserveAnEdge_:: Reserve the desired edge -> Acquire axis lock for bot apc on that axis by AcquireGraphMutationFlag_ -||> Reservs Childs edge-table for update only when initialized : So atleast for now Complication should justify the cost -> PrepareInharitedAxis -> WriteAcquiredAxisDelta_(write both axis) -> PublishReservedEdge_(if the one being linked has owned table then both inhereted and current edge) -> ReleseGraphMutationFlag_(for the one bing linked)
    FAILURE IN ANY POINT :
        Restore axis locks -> Restore owned edge table / child edge table if available

14. ConstructAPCIdentity::UnlinkTwoAPC : 
        I. ReadIdentityBufferOfAPC(the one being delinked) & read the inharited edge idx, predessor edge idx, next edge idx, and own edge_Idx(though redundent because it is fixed removing it atleast need a bitflag to know if ths apc allows own axis attachment or detachment)
        II. ReserveAnEdge_(reserves the roots and (childs own root if avaiavle) edge)
        III. AcquireGraphMutationFlag_(acquire graph mutation flags for predessor, child and next if next is available)
        IV. PrepareForDetachmentOfInharitedAxis
        V. WriteAcquiredAxisDelta_(Write updated identities to APC)
        VI. (publish updated root edge and edge of the one who is being detached if availavle)
        VII. ReleseGraphMutationFlag_(Relese all graph mutation the locks)
    FAILURE IN ANY POINT :
        Revert all the changes happened before and return false

15. APCLifeCycle::SwitchOwnershipOfAReadyDescription : Switches state of a description by checking the 
    the provided DESIRED state and "IsTransitionStateLeagal()"

16. APCLifeCycle::GetASlotForNewAPCLink : Its a toy function giving a first FREE state APC.

17. FabricToAPCLinker::BindExternalRawFabricBacking_ : Attaches APC memory range ptr, Fabrics own ptr .... to APC.

18. ReadAndWriteOfAPC::InitiateAPCMetaHeader : Configures layout, datat types of those layouts and protocol of 
    those layouts. It requires APC state RESRVED so re-configure should reserve before request.

19. VagueTemoraryPremativeFabric::StoreAPCRuntimePtr : stores pointer to temporary(temporary because APC is just a 
    view and because probably we dont need the table) ptr table for APC

20. VagueTemoraryPremativeFabric::CreateAPC : 
    GetASlotForNewAPCLink -> AttachValidIdentity(to the new slo we got) -> BindExternalRawFabricBacking_(to the newly created APC) -> InitiateAPCMetaHeader -> SwitchDescriptionState( switch the description/APC state to LIVE) ->
    ReserveAnEdge_ + InitiateRootAxis(required edges).
    FAILURE IN ANY POINT :
        Rollback the edges -> SwitchDescriptionState(switches the state back to free)

21. AdaptivePackedCellContainer::AttachSiblingOrChild  &  AttachMeToAnother :
    A Thin Wrapper Calling "ConstructAPCIdentity::UnlinkTwoAPC" 

22. AdaptivePackedCellContainer::DetachMyChild & DetachMeFromAnotherEdge :
    A Thin Wrapper Calling "ConstructAPCIdentity::LinkTwoAPC" 

23. ConstructAPCIdentity::ReadGraphMutationFlags : GetSegmentPoolRange + Just atomically reads the Graph mutation flags.

24. ReadAndWriteOfAPC::ReadAPCMetaUnit : should be protected direct access to meta header should be prohibated. It should have extensions to read identity , schema, layout (schema, and layout read can be direct access they are build once or when build the APC IS RESERVED) but identity shoud be validated by ReadGraphMutationFlags

25. AdaptivePackedCellContainer* AdaptivePackedCellContainer::FindPrevious + FindMyNext : Sequence locked topest layer of the ForestS API.

26. FabricConstructor::AtomicallyLoadReadAUnit : reads a uint64_t atomically.

27. FabricConstructor::ReadBufferwithSyncAtomicIndex : Input parameter is the buffer index caller dosent need to know slab index and that index is the only index is being read atomically. ReadASnapShotFromSlab -> AtomicallyLoadReadAUnit

28. IdentityBufferFromSegmentPoolRange : Compiles initial Idintity buffer from  segment pool Range.

28 . VagueTemoraryPremativeFabric::GetAPCRuntimePtrBySlotIndex_ : 

29. RecordBookConstructor::WriteARecordBookOfTSCEntry_ : 


FOUND:
    I will remove "APC_HANDLE_DESCRIPTOR" and just copy its SEQLock + Lifesycle inside The APC itself. Removing one more memory & tyime complexity hogging APC Performance

TEST 1:
    1. APC Link treversal should be in build system and extreamly efficient.


LINE GRAPH:
                         FABRIC
                            |
             +--------------+--------------+
             |                             |
          H FOREST                       V FOREST
             |                             |
         APC A                           APC A
        /  |  \                         /    \
       B   C   D                       X      Y
      / \       \                            |
     E   F       G                           Z