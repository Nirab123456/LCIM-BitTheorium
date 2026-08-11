1. RangeOfAPC APCHandleDescriptorConstructor::GetSegmentPoolRange :: Reads Range of A APC statically not from descriptor the whole architecture is built on this, still keeping track of APC range inside FabricSegments::APC_HANDLE_DESCRIPTOR

2. APCHandleDescriptorConstructor::ReadAPCStateAtomically_ : Should impliment seq lock increment on read

3. bool ConstructAPCIdentity::ReleseGraphMutationFlag_ : Reads APC range by -> GetSegmentPoolRange : checks if the axis available to relese if so releses

4. ConstructAPCIdentity::WriteAcquiredAxis_ : Reads APC range from GetAPCSegmentPoolRange, and reads Mutation flag inside the APC and , Reads current APC state from Description and checks if Description == RESERVED and desired axis Is locked + validates the idintity buffer and just writes only those axis value and returns.

6. EdgeTableConstructor::PublishReservedEdge_ : Checks current edge status = EdgeStatus::RESERVED build a valid Edge buffer with with updated sequense lock and provided data by EdgeBuilder::EdgeData struct and just updates atomically.

5. ConstructAPCIdentity::InitiateRootAxis : 
    (Check EDGE == RESERVED) -> (DESCRIPTION == LIVE) -> (Call To WriteAcquiredAxis_) -> (Call To PublishReservedEdge_)
    FAILURE IN ANY POINT :
    (ReleseGraphMutationFlag_) -> (RELESE: Axis lock by PublishReservedEdge_)