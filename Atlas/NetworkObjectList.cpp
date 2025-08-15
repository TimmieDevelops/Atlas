#include "NetworkObjectList.h"

inline Func* Function = new Func();

void FNetworkObjectList::AddInitialObjects(const UWorld* World, const FName NetDriverName)
{
	static void (*AddInitialObjectsOG)(FNetworkObjectList * This, const UWorld * World, const FName NetDriverName) = decltype(AddInitialObjectsOG)(Function->ImageBase + OFFSET_NetworkObjectListAddInitialObjects);
	return AddInitialObjectsOG(this, World, NetDriverName);
}

void FNetworkObjectList::Remove(const AActor* Actor)
{
	static void (*RemoveOG)(FNetworkObjectList * This, const AActor * Actor) = decltype(RemoveOG)(Function->ImageBase + OFFSET_NetworkObjectListRemove);
	return RemoveOG(this, Actor);
}

void FNetworkObjectList::Reset()
{
	static void (*ResetOG)(FNetworkObjectList * This) = decltype(ResetOG)(Function->ImageBase + OFFSET_NetworkObjectListReset);
}
