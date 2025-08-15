#pragma once
#include "framework.h"

class Globals
{
public:
	inline static float TimeSeconds = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());
	inline static float BusStartTime = 120.0f;
	inline static int32 BeaconPort = 7777;
	inline static ENetMode NetMode = ENetMode::DedicatedServer;
	inline static float MaxTickRate = 30.0f;
};