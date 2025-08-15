#include <algorithm>
#include <vector>
#include "NetworkDriver.h"

FActorPriority::FActorPriority(FNetworkObjectInfo* InActorInfo, UActorChannel* InChannel)
	: ActorInfo(InActorInfo), Channel(InChannel)
{

}

FActorPriority::FActorPriority(class UNetConnection* InConnection, class UActorChannel* InChannel, FNetworkObjectInfo* InActorInfo, const TArray<struct FNetViewer>& Viewers, bool bLowBandwidth)
	: ActorInfo(InActorInfo), Channel(InChannel)
{
	// Proper priority calculation will be done in a separate function
	Priority = 1;
}

inline Globals* Global = new Globals();
inline Func* Function = new Func();
inline Utils* Util = new Utils();

void NetDriver::Hook()
{
	VirtualHook<UNetDriver>(TickFlush, reinterpret_cast<void**>(&TickFlushOG)).Hook(OFFSET_TickFlush);
}

void NetDriver::TickFlush(UNetDriver* Driver, float DeltaSeconds)
{
	NetworkDriver(Driver).ServerReplicateActors(DeltaSeconds);
	return TickFlushOG(Driver, DeltaSeconds);
}

FNetworkObjectList& NetworkDriver::GetNetworkObjectList()
{
	// TODO: insert return statement here
	return *NetworkObjects;
}

FNetViewer NetworkDriver::CreateNetViewer(UNetConnection* Connection)
{
	if (!Connection->OwningActor)
		return FNetViewer();

	if (!(!Connection->PlayerController || (Connection->PlayerController == Connection->OwningActor)))
		return FNetViewer();

	FNetViewer NewViewer{};

	NewViewer.Connection = Connection;
	NewViewer.InViewer = Connection->PlayerController ? Connection->PlayerController : Connection->OwningActor;
	NewViewer.ViewTarget = Connection->ViewTarget;

	APlayerController* ViewingController = Connection->PlayerController;

	NewViewer.ViewLocation = NewViewer.ViewTarget->K2_GetActorLocation();
	if (ViewingController) {
		FRotator ViewRotation = ViewingController->GetControlRotation();

		float CP = cos(UKismetMathLibrary::DegreesToRadians(ViewRotation.Pitch));
		float SP = sin(UKismetMathLibrary::DegreesToRadians(ViewRotation.Pitch));

		float CY = cos(UKismetMathLibrary::DegreesToRadians(ViewRotation.Yaw));
		float SY = sin(UKismetMathLibrary::DegreesToRadians(ViewRotation.Yaw));

		NewViewer.ViewDir = { CP * CY, CP * SY, SP };
	}

	//Util->LOG("NetworkDriver::CreateNetViewer", "Return?");
	return NewViewer;
}

UActorChannel* NetworkDriver::FindChannelRef(UNetConnection* Connection, AActor* Actor)
{
	if (!Connection || !Actor) {
		return nullptr;
	}

	for (int i = 0; i < Connection->OpenChannels.Num(); i++) {
		if (Connection->OpenChannels[i]->IsA(UActorChannel::StaticClass())) {
			UActorChannel* Channel = (UActorChannel*)Connection->OpenChannels[i];
			if (Channel && Channel->Actor == Actor) {
				//Util->LOG("NetworkDriver::FindChannelRef", std::format("Found Actor={}", Actor->GetFullName()).c_str());
				return Channel;
			}
		}
	}

	return nullptr;
}

FName NetworkDriver::GetClientWorldPackageName(const UNetConnection* InConnection) const
{
	return *(FName*)(InConnection->GetOffset() + 0x337B8);
}

bool NetworkDriver::IsLevelInitializedForActor(const AActor* InActor, const UNetConnection* InConnection) const
{
	if (!InActor || !InConnection) {
		return false;
	}

	const bool bCorrectWorld = (GetClientWorldPackageName(InConnection) == Driver->WorldPackage->Name && Function->ClientHasInitializedLevelFor(InConnection, InActor));

	const bool bIsConnectionPC = (InActor == InConnection->PlayerController);

	return bCorrectWorld || bIsConnectionPC;
}

bool NetworkDriver::IsActorRelevantToConnection(const AActor* Actor, const TArray<FNetViewer>& ConnectionViewers)
{
	for (int32 i = 0; i < ConnectionViewers.Num(); i++) {
		if (Function->IsNetRelevantFor(Actor, ConnectionViewers[i].InViewer, ConnectionViewers[i].ViewTarget, ConnectionViewers[i].ViewLocation)) {
			return true;
		}
	}

	return false;
}

void NetworkDriver::CalculatePriority(FActorPriority* Priority, UNetConnection* InConnection, const TArray<FNetViewer>& Viewers, bool bLowBandwidth)
{
	// Simplified priority calculation
	float TimeSinceLastReplication = Driver->Time - Priority->ActorInfo->LastNetReplicateTime;
	float PriorityMultiplier = 1.0f;

	if (TimeSinceLastReplication > 0)
	{
		PriorityMultiplier = UKismetMathLibrary::Sqrt(TimeSinceLastReplication);
	}

	Priority->Priority = (int32)(Priority->ActorInfo->Actor->NetPriority * PriorityMultiplier);
}

UNetConnection* NetworkDriver::IsActorOwnedByAndRelevantToConnection(AActor* Actor, const TArray<FNetViewer>& ConnectionViewers, bool& bOutHasNullViewTarget)
{
	bOutHasNullViewTarget = false;

	for (const FNetViewer& Viewer : ConnectionViewers) {
		if (!Viewer.Connection || !Viewer.InViewer || !Viewer.ViewTarget) {
			bOutHasNullViewTarget = true;
			continue;
		}

		if (Viewer.Connection->PlayerController &&
			(Viewer.Connection->PlayerController == Actor || Viewer.Connection->PlayerController->Pawn == Actor))
		{
			return Viewer.Connection;
		}

		if (Actor->Owner && Actor->Owner == Viewer.InViewer) {
			return Viewer.Connection;
		}
	}

	return nullptr;
}

int32 NetworkDriver::ServerReplicateActors(float DeltaSeconds)
{
	if (Driver->ClientConnections.Num() == 0) {
		return 0;
	}

	if (!Driver->World) {
		return 0;
	}

	int32 Updated = 0;

	ReplicationFrame++;

	const int32 NumClientsToTick = PrepConnections();
	if (NumClientsToTick == 0) {
		return 0;
	}

	float ServerTickTime = (Global->MaxTickRate == 0.f) ? DeltaSeconds : (1.f / Global->MaxTickRate);

	TArray<FNetworkObjectInfo*> ConsiderList;
	ConsiderList.ResizeTo(GetNetworkObjectList().GetActiveObjects().Num());

	BuildConsiderList(ConsiderList, ServerTickTime);

	for (int32 i = 0; i < Driver->ClientConnections.Num(); i++) {
		UNetConnection* Connection = Driver->ClientConnections[i];
		if (!Connection) {
			continue;
		}

		if (i >= NumClientsToTick) {
			continue;
		}

		if (!Connection->ViewTarget) {
			continue;
		}

		TArray<FNetViewer> ConnectionViewers;

		ConnectionViewers.Free();
		ConnectionViewers.Add(CreateNetViewer(Connection));
		for (int32 ChildIdx = 0; ChildIdx < Connection->Children.Num(); ChildIdx++) {
			if (Connection->Children[ChildIdx]->ViewTarget != NULL) {
				ConnectionViewers.Add(CreateNetViewer(Connection->Children[ChildIdx]));
			}
		}

		if (Connection->PlayerController) {
			Function->SendClientAdjustment(Connection->PlayerController);
		}

		for (int32 ChildIdx = 0; ChildIdx < Connection->Children.Num(); ChildIdx++) {
			if (Connection->Children[ChildIdx]->PlayerController != NULL) {
				Function->SendClientAdjustment(Connection->Children[ChildIdx]->PlayerController);
			}
		}

		// New actor processing logic
		std::vector<FActorPriority*> PriorityList;
		PriorityList.reserve(ConsiderList.Num());

		for (FNetworkObjectInfo* ActorInfo : ConsiderList) {
			if (!ActorInfo || !ActorInfo->Actor || ActorInfo->Actor->bActorIsBeingDestroyed)
			{
				continue;
			}

			AActor* Actor = ActorInfo->Actor;
			bool bIsRelevant = IsActorRelevantToConnection(Actor, ConnectionViewers);

			if (bIsRelevant)
			{
				UActorChannel* Channel = FindChannelRef(Connection, Actor);
				if (!Channel)
				{
					bool bOutHasNullViewTarget = false;
					UNetConnection* OwningConnection = IsActorOwnedByAndRelevantToConnection(Actor, ConnectionViewers, bOutHasNullViewTarget);

					if (OwningConnection)
					{
						Channel = (UActorChannel*)Function->CreateChannel(Connection, 2, true, -1);
						if (Channel)
						{
							Function->SetChannelActor(Channel, Actor);
						}
					}
				}

				if (Channel)
				{
					// Add to priority list
					FActorPriority* Priority = new FActorPriority(Connection, Channel, ActorInfo, ConnectionViewers, false);
					CalculatePriority(Priority, Connection, ConnectionViewers, false);
					PriorityList.push_back(Priority);
				}
			}
		}

		std::sort(PriorityList.begin(), PriorityList.end(), FCompareFActorPriority());

		for (FActorPriority* Priority : PriorityList)
		{
			if (Priority && Priority->Channel)
			{
				Function->ReplicateActor(Priority->Channel);
				Updated++;
			}
		}

		// Cleanup the priority list
		for (FActorPriority* Priority : PriorityList)
		{
			delete Priority;
		}
		PriorityList.clear();
	}

	return Updated;
}

int32 NetworkDriver::PrepConnections()
{
	int32 NumClientsToTick = Driver->ClientConnections.Num();

	bool bFoundReadyConnection = false;

	for (int32 i = 0; i < NumClientsToTick; i++) {
		UNetConnection* Connection = Driver->ClientConnections[i];
		if (!Connection) {
			continue;
		}

		AActor* OwningActor = Connection->OwningActor;
		if (OwningActor != NULL && (Connection->Driver->Time - Connection->LastReceiveTime < 1.5f)) {
			bFoundReadyConnection = true;

			AActor* DesiredViewTarget = OwningActor;
			if (Connection->PlayerController) {
				if (AActor* ViewTarget = Connection->PlayerController->GetViewTarget()) {
					DesiredViewTarget = ViewTarget;
				}
			}

			Connection->ViewTarget = DesiredViewTarget;

			for (int32 ChildIdx = 0; ChildIdx < Connection->Children.Num(); ChildIdx++) {
				UNetConnection* Child = Connection->Children[ChildIdx];
				if (Child->PlayerController != NULL) {
					Child->ViewTarget = Child->PlayerController->GetViewTarget();
				}
				else {
					Child->ViewTarget = NULL;
				}
			}
		}
		else {
			Connection->ViewTarget = NULL;
			for (int32 ChildIdx = 0; ChildIdx < Connection->Children.Num(); ChildIdx++) {
				Connection->Children[ChildIdx]->ViewTarget = NULL;
			}
		}
	}

	return bFoundReadyConnection ? NumClientsToTick : 0;
}

void NetworkDriver::BuildConsiderList(TArray<FNetworkObjectInfo*>& OutConsiderList, const float ServerTickTime)
{
	TArray<AActor*> ActorsToRemove;

	for (const TSharedPtr<FNetworkObjectInfo>& ObjectInfo : GetNetworkObjectList().GetActiveObjects()) {
		FNetworkObjectInfo* ActorInfo = ObjectInfo.Get();

		if (!ActorInfo->bPendingNetUpdate && Global->TimeSeconds <= ActorInfo->NextUpdateTime) {
			continue;
		}

		AActor* Actor = ActorInfo->Actor;

		if (Actor->bActorIsBeingDestroyed || Actor->GetRemoteRole() == ENetRole::ROLE_None) {
			ActorsToRemove.Add(Actor);
			continue;
		}

		if (Actor->NetDriverName != Driver->NetDriverName) {
			continue;
		}

		if (Actor->NetDormancy == ENetDormancy::DORM_Initial && Actor->bNetStartup) {
			ActorsToRemove.Add(Actor);
			continue;
		}

		if (ActorInfo->LastNetReplicateTime == 0) {
			ActorInfo->LastNetReplicateTime = Global->TimeSeconds;
			ActorInfo->OptimalNetUpdateDelta = 1.0f / Actor->NetUpdateFrequency;
		}

		const float ScaleDownStartTime = 2.0f;
		const float ScaleDownTimeRange = 5.0f;

		const float LastReplicateDelta = Global->TimeSeconds - ActorInfo->LastNetReplicateTime;

		if (LastReplicateDelta > ScaleDownStartTime) {
			if (Actor->MinNetUpdateFrequency == 0.0f) {
				Actor->MinNetUpdateFrequency = 2.0f;
			}

			const float MinOptimalDelta = 1.0f / Actor->NetUpdateFrequency;
			const float MaxOptimalDelta = UKismetMathLibrary::Max(1.0f / Actor->MinNetUpdateFrequency, MinOptimalDelta);

			const float Alpha = UKismetMathLibrary::Clamp((LastReplicateDelta - ScaleDownStartTime) / ScaleDownTimeRange, 0.0f, 1.0f);
			ActorInfo->OptimalNetUpdateDelta = UKismetMathLibrary::Lerp(MinOptimalDelta, MaxOptimalDelta, Alpha);
		}

		if (!ActorInfo->bPendingNetUpdate) {
			const float NextUpdateDelta = 1.0f / Actor->NetUpdateFrequency;

			ActorInfo->NextUpdateTime = Global->TimeSeconds + UKismetMathLibrary::RandomFloatInRange(0, 1) * ServerTickTime + NextUpdateDelta;

			ActorInfo->LastNetUpdateTime = Driver->Time;
		}

		ActorInfo->bPendingNetUpdate = false;

		OutConsiderList.Add(ActorInfo);

		Function->CallPreReplication(Actor, Driver);
	}

	for (AActor* Actor : ActorsToRemove) {
		GetNetworkObjectList().Remove(Actor);
	}

	ActorsToRemove.Free();
}