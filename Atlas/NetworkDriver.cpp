#include "NetworkDriver.h"

inline Globals* Global = new Globals();
inline Func* Function = new Func();
inline Utils* Util = new Utils();

void NetDriver::Hook()
{
	VirtualHook<UNetDriver>(TickFlush, reinterpret_cast<void**>(&TickFlushOG)).Hook(OFFSET_TickFlush);
}

// https://github.com/EpicGames/UnrealEngine/blob/6978b63c8951e57d97048d8424a0bebd637dde1d/Engine/Source/Runtime/Engine/Private/NetDriver.cpp#L1038
void NetDriver::TickFlush(UNetDriver* Driver, float DeltaSeconds)
{
	NetworkDriver(Driver).ServerReplicateActors(DeltaSeconds);
	return TickFlushOG(Driver, DeltaSeconds);
}

// https://github.com/EpicGames/UnrealEngine/blob/6978b63c8951e57d97048d8424a0bebd637dde1d/Engine/Source/Runtime/Engine/Classes/Engine/NetDriver.h#L1876
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

// https://github.com/EpicGames/UnrealEngine/blob/6978b63c8951e57d97048d8424a0bebd637dde1d/Engine/Source/Runtime/Engine/Private/NetDriver.cpp#L8113
bool NetworkDriver::IsDormInitialStartupActor(AActor* Actor)
{
	return Actor && Actor->bNetStartup && (Actor->NetDormancy == ENetDormancy::DORM_Initial);
}

UActorChannel* NetworkDriver::FindActorChannelRef(UNetConnection* Connection, AActor* Actor)
{
	if (!Connection || !Actor) {
		return nullptr;
	}

	for (UChannel* Channel : Connection->OpenChannels) {
		UActorChannel* ActorChannel = Channel->Cast<UActorChannel>();
		if (ActorChannel && ActorChannel->Actor == Actor) {
			return ActorChannel;
		}
	}

	return nullptr;
}

// https://github.com/EpicGames/UnrealEngine/blob/ea87f4fb26ff8b9d8cd49b3a930f10585a6a7230/Engine/Source/Runtime/Engine/Classes/Engine/NetConnection.h#L395
FName NetworkDriver::GetClientWorldPackageName(const UNetConnection* InConnection) const
{
	return *(FName*)(InConnection->GetOffset() + 0x337B8);
}

// https://github.com/EpicGames/UnrealEngine/blob/6978b63c8951e57d97048d8424a0bebd637dde1d/Engine/Source/Runtime/Engine/Private/NetDriver.cpp#L2650
bool NetworkDriver::IsLevelInitializedForActor(const AActor* InActor, const UNetConnection* InConnection) const
{
	if (!InActor || !InConnection) {
		return false;
	}

	return Function->ClientHasInitializedLevelFor(InConnection, InActor);
}

// https://github.com/EpicGames/UnrealEngine/blob/6978b63c8951e57d97048d8424a0bebd637dde1d/Engine/Source/Runtime/Engine/Private/NetDriver.cpp#L5337
bool NetworkDriver::IsActorRelevantToConnection(const AActor* Actor, const TArray<FNetViewer>& ConnectionViewers)
{
	for (int32 ViewerIdx = 0; ViewerIdx < ConnectionViewers.Num(); ViewerIdx++) {
		if (Function->IsNetRelevantFor(Actor, ConnectionViewers[ViewerIdx].InViewer, ConnectionViewers[ViewerIdx].ViewTarget, ConnectionViewers[ViewerIdx].ViewLocation)) {
			return true;
		}
	}

	return false;
}

// https://github.com/EpicGames/UnrealEngine/blob/6978b63c8951e57d97048d8424a0bebd637dde1d/Engine/Source/Runtime/Engine/Private/NetDriver.cpp#L6057
int32 NetworkDriver::ServerReplicateActors(float DeltaSeconds)
{
	if (this->Driver->ClientConnections.Num() == 0) {
		return 0;
	}

	if (!Driver->World) {
		return 0;
	}

	this->ReplicationFrame++;

	int32 Updated = 0;

	const int32 NumClientsToTick = this->PrepConnections();
	if (!NumClientsToTick) {
		return 0;
	}

	float ServerTickTime = (Global->MaxTickRate == 0.f) ? DeltaSeconds : (1.f / Global->MaxTickRate);

	TArray<FNetworkObjectInfo*> ConsiderList;
	ConsiderList.ResizeTo(GetNetworkObjectList().GetAllObjects().Num());

	this->BuildConsiderList(ConsiderList, ServerTickTime);

	TSet<UNetConnection*> ConnectionsToClose;

	for (int32 i = 0; i < this->Driver->ClientConnections.Num(); i++) {
		UNetConnection* Connection = this->Driver->ClientConnections[i];
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
		for (int32 ViewerIndex = 0; ViewerIndex < Connection->Children.Num(); ViewerIndex++) {
			if (Connection->Children[ViewerIndex]->ViewTarget) {
				ConnectionViewers.Add(CreateNetViewer(Connection->Children[ViewerIndex]));
			}
		}

		if (Connection->PlayerController) {
			Function->SendClientAdjustment(Connection->PlayerController);
		}

		for (int32 ChildIdx = 0; ChildIdx < Connection->Children.Num(); ChildIdx++) {
			if (Connection->Children[ChildIdx]->PlayerController) {
				Function->SendClientAdjustment(Connection->Children[ChildIdx]->PlayerController);
			}
		}

		FActorPriority* PriorityList = 0; // NULL
		FActorPriority** PriorityActors = 0; // NULL

		const int32 FinalSortedCount = this->PrioritizeActors(Connection, ConnectionViewers, ConsiderList, PriorityList, PriorityActors);
		const int32 LastProcessedActor = this->ProcessPrioritizedActorsRange(Connection, ConnectionViewers, PriorityActors, FinalSortedCount, Updated);
	}

	return Updated;
}

// https://github.com/EpicGames/UnrealEngine/blob/6978b63c8951e57d97048d8424a0bebd637dde1d/Engine/Source/Runtime/Engine/Private/NetDriver.cpp#L5077
int32 NetworkDriver::PrepConnections()
{
	int32 NumClientsToTick = this->Driver->ClientConnections.Num();

	bool bFoundReadyConnection = false;

	for (int32 ConnIdx = 0; ConnIdx < NumClientsToTick; ConnIdx++) {
		UNetConnection* Connection = this->Driver->ClientConnections[ConnIdx];
		if (!Connection) {
			continue;
		}

		AActor* OwningActor = Connection->OwningActor;
		if (OwningActor && (Connection->Driver->Time - Connection->LastReceiveTime < 1.5f)) {
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
				APlayerController* ChildPlayerController = Child->PlayerController;
				AActor* DesiredChildViewTarget = Child->OwningActor;
				if (ChildPlayerController) {
					AActor* ChildViewTarget = ChildPlayerController->GetViewTarget();
					if (ChildViewTarget) {
						DesiredChildViewTarget = ChildViewTarget;
					}
				}
				Child->ViewTarget = DesiredChildViewTarget;
			}
		}
		else {
			Connection->ViewTarget = nullptr;
			for (int32 ChildIdx = 0; ChildIdx < Connection->Children.Num(); ChildIdx++) {
				Connection->Children[ChildIdx]->ViewTarget = nullptr;
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

		if (this->IsDormInitialStartupActor(Actor)) {
			ActorsToRemove.Add(Actor);
			continue;
		}

		if (!ActorInfo->LastNetReplicateTime) {
			ActorInfo->LastNetReplicateTime = Global->TimeSeconds;
			ActorInfo->OptimalNetUpdateDelta = 1.0f / Actor->NetUpdateFrequency;
		}

		const float ScaleDownStartTime = 2.0f;
		const float ScaleDownTimeRange = 5.0f;
		const float LastReplicateDelta = Global->TimeSeconds - ActorInfo->LastNetReplicateTime;

		if (LastReplicateDelta > ScaleDownStartTime) {
			if (Actor->MinNetUpdateFrequency == 0.0f) {
				Actor->MinNetUpdateFrequency = 0.0f;
			}

			const float MinOptimalDelta = 1.0f / Actor->NetUpdateFrequency;
			const float MaxOptimalDelta = UKismetMathLibrary::Max(1.0f / Actor->MinNetUpdateFrequency, MinOptimalDelta);
			const float Alpha = UKismetMathLibrary::Clamp((LastReplicateDelta - ScaleDownStartTime) / ScaleDownTimeRange, 0.0f, 1.0f);
			ActorInfo->OptimalNetUpdateDelta = UKismetMathLibrary::Lerp(MinOptimalDelta, MaxOptimalDelta, Alpha);
		}

		if (!ActorInfo->bPendingNetUpdate) {
			const float NextUpdateDelta = 1.0f / Actor->NetUpdateFrequency;
			float RandDelay = Rand() * ServerTickTime;

			ActorInfo->NextUpdateTime = Global->TimeSeconds + RandDelay + NextUpdateDelta;
			ActorInfo->LastNetUpdateTime = Driver->Time;
		}

		ActorInfo->bPendingNetUpdate = false;

		OutConsiderList.Add(ActorInfo);

		Function->CallPreReplication(Actor, this->Driver);
	}

	for (AActor* Actor : ActorsToRemove) {
		GetNetworkObjectList().Remove(Actor);
	}

	ActorsToRemove.Free();
}

int32 NetworkDriver::PrioritizeActors(UNetConnection* Connection, const TArray<FNetViewer>& ConnectionViewers, const TArray<FNetworkObjectInfo*>& ConsiderList, FActorPriority*& OutPriorityList, FActorPriority**& OutPriorityActors)
{
	int32 FinalSortedCount = 0;

	const int32 MaxSortedActors = ConsiderList.Num();
	if (MaxSortedActors > 0) {
		OutPriorityList = new FActorPriority[MaxSortedActors];
		OutPriorityActors = new FActorPriority * [MaxSortedActors];

		for (FNetworkObjectInfo* ActorInfo : ConsiderList) {
			AActor* Actor = ActorInfo->Actor;
			if (!Actor) {
				continue;
			}

			UActorChannel* Channel = this->FindActorChannelRef(Connection, Actor);

			if (!Channel && !this->IsActorRelevantToConnection(Actor, ConnectionViewers)) {
				continue;
			}

			OutPriorityList[FinalSortedCount] = FActorPriority(ActorInfo, Channel);
			OutPriorityList[FinalSortedCount].Priority = static_cast<int32>(UKismetMathLibrary::Clamp(1000000.0f - (Actor->K2_GetActorLocation() - ConnectionViewers[0].ViewLocation).SizeSquared(), 0.0f, 1000000.0f));
			OutPriorityActors[FinalSortedCount] = &OutPriorityList[FinalSortedCount];

			FinalSortedCount++;
		}

		std::sort(OutPriorityActors, OutPriorityActors + FinalSortedCount, FCompareFActorPriority());
	}

	return FinalSortedCount;
}

// https://github.com/EpicGames/UnrealEngine/blob/6978b63c8951e57d97048d8424a0bebd637dde1d/Engine/Source/Runtime/Engine/Private/NetDriver.cpp#L5558
int32 NetworkDriver::ProcessPrioritizedActorsRange(UNetConnection* Connection, const TArray<FNetViewer>& ConnectionViewers, FActorPriority** PriorityActors, const int32 FinalSortedCount, int32& OutUpdated)
{
	int32 FinalRelevantCount = 0;

	for (int32 j = 0; j < FinalSortedCount; j++) {
		FNetworkObjectInfo* ActorInfo = PriorityActors[j]->ActorInfo;
		if (!ActorInfo || !ActorInfo->Actor) {
			continue;
		}

		AActor* Actor = ActorInfo->Actor;
		UActorChannel* Channel = this->FindActorChannelRef(Connection, Actor);

		if (!Channel) {
			Channel = (UActorChannel*)Function->CreateChannel(Connection, 2, true, -1);
			if (Channel) {
				Function->SetChannelActor(Channel, Actor);
			}
		}

		if (Channel && Function->ReplicateActor(Channel)) {
			OutUpdated++;
			FinalRelevantCount++;
		}
	}

	return FinalRelevantCount;
}
