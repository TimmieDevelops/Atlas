#pragma once
#include "framework.h"
#include "VirtualHook.h"
#include "Globals.h"
#include "NetworkObjectList.h"
#include "Utils.h"

//
// Priority sortable list.
//
struct FActorPriority
{
	int32						Priority;	// Update priority, higher = more important.

	FNetworkObjectInfo* ActorInfo;	// Actor info.
	UActorChannel* Channel;	// Actor channel.

	//struct FActorDestructionInfo* DestructionInfo;	// Destroy an actor

	FActorPriority() :
		Priority(0), ActorInfo(NULL), Channel(NULL)
	{
	}

	FActorPriority(FNetworkObjectInfo* InActorInfo, UActorChannel* InChannel) :
		ActorInfo(InActorInfo), Channel(InChannel)
	{

	}

	//FActorPriority(class UNetConnection* InConnection, class UActorChannel* InChannel, FNetworkObjectInfo* InActorInfo, const TArray<struct FNetViewer>& Viewers, bool bLowBandwidth);
	//FActorPriority(class UNetConnection* InConnection, struct FActorDestructionInfo* DestructInfo, const TArray<struct FNetViewer>& Viewers);
};

struct FCompareFActorPriority
{
	FORCEINLINE bool operator()(const FActorPriority* A, const FActorPriority* B) const
	{
		return B->Priority < A->Priority;
	}
};

class NetDriver
{
private:
	inline static void (*TickFlushOG)(UNetDriver* Driver, float DeltaSeconds);
public:
	static void Hook();
	static void TickFlush(UNetDriver* Driver, float DeltaSeconds);
};

class NetworkDriver
{
public:
	UNetDriver* Driver;
	int32 ReplicationFrame;
	TSharedPtr<FNetworkObjectList> NetworkObjects;

	NetworkDriver(UNetDriver* InDriver) :
		Driver(InDriver)
	{
		ReplicationFrame = *(int32*)(InDriver->GetOffset() + 0x2C8);
		NetworkObjects = *(TSharedPtr<FNetworkObjectList>*)(InDriver->GetOffset() + 0x490);
	}
private:
	FNetworkObjectList& GetNetworkObjectList();
	FNetViewer CreateNetViewer(UNetConnection* Connection);
	bool IsDormInitialStartupActor(AActor* Actor);
	UActorChannel* FindActorChannelRef(UNetConnection* Connection, AActor* Actor);
	FName GetClientWorldPackageName(const UNetConnection* InConnection) const;
	bool IsLevelInitializedForActor(const AActor* InActor, const UNetConnection* InConnection) const;
	static bool IsActorRelevantToConnection(const AActor* Actor, const TArray<FNetViewer>& ConnectionViewers);
public:
	int32 ServerReplicateActors(float DeltaSeconds);
	int32 PrepConnections();
	void BuildConsiderList(TArray<FNetworkObjectInfo*>& OutConsiderList, const float ServerTickTime);
	int32 PrioritizeActors(UNetConnection* Connection, const TArray<FNetViewer>& ConnectionViewers, const TArray<FNetworkObjectInfo*>& ConsiderList, FActorPriority*& OutPriorityList, FActorPriority**& OutPriorityActors);
	int32 ProcessPrioritizedActorsRange(UNetConnection* Connection, const TArray<FNetViewer>& ConnectionViewers, FActorPriority** PriorityActors, const int32 FinalSortedCount, int32& OutUpdated);
};