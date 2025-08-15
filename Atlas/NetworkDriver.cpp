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
	static bool bFirstCall = true;
	if (bFirstCall)
	{
		Util->LOG("ServerReplicateActors", "First call");
		bFirstCall = false;
	}

	ReplicationFrame++;

	const int32 NumClientsToTick = PrepConnections();
	if (NumClientsToTick == 0)
	{
		return 0;
	}

	float ServerTickTime = (Global->MaxTickRate == 0.f) ? DeltaSeconds : (1.f / Global->MaxTickRate);

	TArray<FNetworkObjectInfo*> ConsiderList;
	ConsiderList.Reserve(GetNetworkObjectList().GetActiveObjects().Num());
	BuildConsiderList(ConsiderList, ServerTickTime);

	int32 Updated = 0;

	for (int32 i = 0; i < Driver->ClientConnections.Num(); i++)
	{
		UNetConnection* Connection = Driver->ClientConnections[i];
		if (!Connection || !Connection->ViewTarget)
		{
			continue;
		}

		if (Connection->PlayerController)
		{
			Function->SendClientAdjustment(Connection->PlayerController);
		}

		for (FNetworkObjectInfo* ActorInfo : ConsiderList)
		{
			if (!ActorInfo || !ActorInfo->Actor)
			{
				continue;
			}

			AActor* Actor = ActorInfo->Actor;
      
			UActorChannel* Channel = FindChannelRef(Connection, Actor);

			TArray<FNetViewer> ConnectionViewers;
			ConnectionViewers.Add(CreateNetViewer(Connection));

			bool bIsRelevant = Actor->bAlwaysRelevant || IsActorRelevantToConnection(Actor, ConnectionViewers);

			if (!bIsRelevant && Actor->Owner)
			{
				// Check if relevant to owner
				if (UNetConnection* OwningConnection = IsActorOwnedByAndRelevantToConnection(Actor, ConnectionViewers, *(bool*)((uintptr_t)Connection + 0x1A80)))
				{
					if (OwningConnection == Connection)
					{
						bIsRelevant = true;
					}
				}
			}

			if (!bIsRelevant)
			{
				if (Channel)
				{
					// Close the channel if the actor is no longer relevant
					Function->ActorChannelClose(Channel);
				}
				continue;
			}

			if (!Channel)
			{
				// Create a channel if one doesn't exist
				Channel = (UActorChannel*)Function->CreateChannel(Connection, 2, true, -1);
				if (Channel)
				{
					Function->SetChannelActor(Channel, Actor);
				}
			}

			if (Channel)
			{
				Function->ReplicateActor(Channel);
				ActorInfo->LastNetReplicateTime = Driver->Time;
				ActorInfo->NextUpdateTime = Driver->Time + ActorInfo->OptimalNetUpdateDelta;
				Updated++;
			}
		}
	}

	return Updated;
}

int32 NetworkDriver::PrepConnections()
{
	int32 NumClientsToTick = 0;
	for (int32 i = 0; i < Driver->ClientConnections.Num(); i++)
	{
		UNetConnection* Connection = Driver->ClientConnections[i];
		if (Connection)
		{
			AActor* OwningActor = Connection->OwningActor;
			if (OwningActor && (Driver->Time - Connection->LastReceiveTime < 1.5f))
			{
				// Set the view target
				if (Connection->PlayerController)
				{
					Connection->ViewTarget = Connection->PlayerController->GetViewTarget();
				}
				else
				{
					Connection->ViewTarget = OwningActor;
				}

				// Also set view target for child connections
				for (int32 ChildIdx = 0; ChildIdx < Connection->Children.Num(); ChildIdx++)
				{
					UNetConnection* Child = Connection->Children[ChildIdx];
					if (Child && Child->PlayerController)
					{
						Child->ViewTarget = Child->PlayerController->GetViewTarget();
					}
					else if(Child)
					{
						Child->ViewTarget = nullptr;
					}
				}
				NumClientsToTick++;
			}
			else
			{
				Connection->ViewTarget = nullptr;
				for (int32 ChildIdx = 0; ChildIdx < Connection->Children.Num(); ChildIdx++)
				{
					if(Connection->Children[ChildIdx])
					{
						Connection->Children[ChildIdx]->ViewTarget = nullptr;
					}
				}
			}
		}
	}
	return NumClientsToTick;
}

void NetworkDriver::BuildConsiderList(TArray<FNetworkObjectInfo*>& OutConsiderList, const float ServerTickTime)
{
	TArray<AActor*> ActorsToRemove;

	for (const TSharedPtr<FNetworkObjectInfo>& ObjectInfo : GetNetworkObjectList().GetActiveObjects())
	{
		if (!ObjectInfo.Get())
		{
			continue;
		}

		FNetworkObjectInfo* ActorInfo = ObjectInfo.Get();
		AActor* Actor = ActorInfo->Actor;

		if (!Actor || Actor->bActorIsBeingDestroyed || Actor->GetRemoteRole() == ENetRole::ROLE_None)
		{
			ActorsToRemove.Add(Actor);
			continue;
		}

		if (Actor->NetDriverName != Driver->NetDriverName)
		{
			continue;
		}

		// Adjust update frequency and check if actor should be considered for replication
		if (ActorInfo->LastNetReplicateTime == 0)
		{
			ActorInfo->LastNetReplicateTime = Global->TimeSeconds;
			ActorInfo->OptimalNetUpdateDelta = 1.0f / Actor->NetUpdateFrequency;
		}

		const float ScaleDownStartTime = 2.0f;
		const float ScaleDownTimeRange = 5.0f;
		const float LastReplicateDelta = Global->TimeSeconds - ActorInfo->LastNetReplicateTime;

		if (LastReplicateDelta > ScaleDownStartTime)
		{
			if (Actor->MinNetUpdateFrequency == 0.0f)
			{
				Actor->MinNetUpdateFrequency = 2.0f;
			}

			const float MinOptimalDelta = 1.0f / Actor->NetUpdateFrequency;
			const float MaxOptimalDelta = UKismetMathLibrary::Max(1.0f / Actor->MinNetUpdateFrequency, MinOptimalDelta);

			const float Alpha = UKismetMathLibrary::Clamp((LastReplicateDelta - ScaleDownStartTime) / ScaleDownTimeRange, 0.0f, 1.0f);
			ActorInfo->OptimalNetUpdateDelta = UKismetMathLibrary::Lerp(MinOptimalDelta, MaxOptimalDelta, Alpha);
		}

		if (Global->TimeSeconds >= ActorInfo->NextUpdateTime)
		{
			OutConsiderList.Add(ActorInfo);
			Function->CallPreReplication(Actor, Driver);
		}
	}

	for (AActor* Actor : ActorsToRemove)
	{
		GetNetworkObjectList().Remove(Actor);
	}
	ActorsToRemove.Free();
}