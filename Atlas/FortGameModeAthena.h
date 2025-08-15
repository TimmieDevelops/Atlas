#pragma once
#include "framework.h"
#include "VirtualHook.h"
#include "Utils.h"
#include "Globals.h"

class FortGameModeAthena
{
private:
	inline static bool (*ReadyToStartMatchOG)(AGameMode* GameMode);
public:
	void Hook();
	static bool ReadyToStartMatch(AFortGameModeAthena* GameMode);
	static APawn* SpawnDefaultPawnFor(AGameModeBase* GameMode, AController* NewPlayer, AActor* StartSpot);
	static void HandleStartingNewPlayer(AGameModeBase* GameMode, AAthena_PlayerController_C* NewPlayer);
};