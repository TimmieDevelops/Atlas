#pragma once
#include "framework.h"
#include "VirtualHook.h"
#include "Utils.h"

class FortPlayerControllerAthena
{
private:
	inline static void (*GetPlayerViewPointOG)(const APlayerController* PC, FVector& out_Location, FRotator& out_Rotation);
public:
	static void Hook();
	static void GetPlayerViewPoint(const APlayerController* PC, FVector& out_Location, FRotator& out_Rotation);
	static void ServerAcknowledgePossession(APlayerController* PC, APawn* P);
	static void ServerLoadingScreenDropped(AFortPlayerController* PC);
};