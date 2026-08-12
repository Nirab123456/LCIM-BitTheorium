1. RangeOfAPC APCHandleDescriptorConstructor::GetSegmentPoolRange :: Reads Range of A APC statically not from descriptor the whole architecture is built on this, still keeping track of APC range inside FabricSegments::APC_HANDLE_DESCRIPTOR

2. APCHandleDescriptorConstructor::ReadAPCStateAtomically_ : Reads state of the APC 

3. bool ConstructAPCIdentity::ReleseGraphMutationFlag_ : Reads APC range by -> GetSegmentPoolRange : checks if the axis available to relese if so releses

4. ConstructAPCIdentity::WriteAcquiredAxis_ : Reads APC range from GetAPCSegmentPoolRange, and reads Mutation flag inside the APC and , Reads current APC state from Description and checks if Description == RESERVED and desired axis Is locked + validates the idintity buffer and just writes only those axis value and returns.

6. EdgeTableConstructor::PublishReservedEdge_ : Checks current edge status = EdgeStatus::RESERVED build a valid Edge buffer with with updated sequense lock and provided data by EdgeBuilder::EdgeData struct and just updates atomically.

5. ConstructAPCIdentity::InitiateRootAxis : 
    (Check EDGE == RESERVED) -> (DESCRIPTION == LIVE) -> (Call To WriteAcquiredAxis_) -> (Call To PublishReservedEdge_)
    FAILURE IN ANY POINT :
    (ReleseGraphMutationFlag_) -> (RELESE: Axis lock by PublishReservedEdge_)

6. GetMemGFlagFromAxis : Returns the enum flug which should be truned ON/OFF based on the provided axis.

7. FabricConstructor::ReadASnapShotFromSlab : Reades a range in slab to provided buffer based on atomic_required. if
    atomic_required == true then atomically other wise -> std::memcpy.
    Feature / Issue -> Only the last index is Atomic.

8. InstallAxisToBuffer:ValidateIdentityBuffer : ValidateDefaultIdentity + IsValidGraphMutationState.

9. std::optional<StateOfAPC> ConstructAPCIdentity::ReadIdentityBufferOfAPC : 
    I.GetSegmentPoolRange + ReadAPCStateAtomically_ :: Failure in either returns std::nullopt means read operation itself is invalid,
    II. ReadASnapShotFromSlab -> with APCDataStructure::TotalIdentityUnitCount() + ValidateIdentityBuffer :: If either
    fails returns std::nullopt
    OVERVIEW:
        if return is std::nullopt it dosent means structure has failure it simply means it cant verify enough relation to read idintity buffer. Though the function allows read of the idintity buffer regardless of the state of apc thats the reason why state of apc is returened insted of simple boolean.

10. EdgeTableConstructor::SwitchEdgeState__ : Checks if the switch rfequest valid in contrast to current status.
    EXTENSIONS:
        I. ReserveAnEdge_

11. ConstructAPCIdentity::AcquireGraphMutationFlag_ : GetSegmentPoolRange + ReadAPCStateAtomically_ : varifies the desired 
    mutation flag In contrast to current mutation flags if valid acquires the flag.

13. PrepareInharitedAxis : Prepares both identity + Both Edge Table Data EdgeData (owner edge which is bing muted and if 
    the current APC who is bing linked and unlinked to a owner is also a owner then prepers that edge too)

12. ConstructAPCIdentity::LinkTwoAPC : 
    ReadIdentityBufferOfAPC -> ReserveAnEdge_:: Reserve the desired edge -> Acquire axis lock for bot apc on that axis by AcquireGraphMutationFlag_ -||> Reservs Childs edge-table for update only when initialized : So atleast for now Complication should justify the cost -> PrepareInharitedAxis -> WriteAcquiredAxis_(write both axis) -> PublishReservedEdge_(if the one bing linked has owned table then both) -> ReleseGraphMutationFlag_(for the one bing linked)
    FAILURE IN ANY POINT :
        Restore axis locks -> Restore owned edge table / child edge table if available