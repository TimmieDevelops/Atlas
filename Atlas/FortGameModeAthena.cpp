#include "FortGameModeAthena.h"
#include "Misc.h"

inline Utils* Util = new Utils();
inline Globals* Global = new Globals();
inline Func* Function = new Func();

void FortGameModeAthena::Hook()
{
	VirtualHook<AFortGameModeAthena>(ReadyToStartMatch, reinterpret_cast<void**>(&ReadyToStartMatchOG)).VFT(249);
	VirtualHook<AFortGameModeAthena>(SpawnDefaultPawnFor).VFT(191);
	VirtualHook<AFortGameModeAthena>(HandleStartingNewPlayer).VFT(197);
}

bool FortGameModeAthena::ReadyToStartMatch(AFortGameModeAthena* GameMode)
{
	static bool bInitialize = false;
	static bool bIsNetReady = false;

	AFortGameStateAthena* GameState = reinterpret_cast<AFortGameStateAthena*>(GameMode->GameState);
	if (!GameMode || !GameState) return ReadyToStartMatchOG(GameMode);
	if (!GameState->MapInfo) return ReadyToStartMatchOG(GameMode);

	if (!bInitialize)
	{
		bInitialize = true;
		Util->LOG("FortGameModeAthena::ReadyToStartMatch", std::format("bInitialize={}", bInitialize).c_str());

		int32 PlaylistId = 0; // SOLO = 0, DUOS = 1, SQUADS = 3

		GameMode->bWorldIsReady = true;
		GameMode->bUseRandomTimeOfDay = true;
		GameMode->bAllowSpectateAfterDeath = true;
		GameMode->CurrentPlaylistId = PlaylistId;

		AFortGameSessionDedicated* GameSession = Util->SpawnActor<AFortGameSessionDedicated>();
		GameSession->SessionName = UKismetStringLibrary::Conv_StringToName(L"Atlas");
		GameSession->MaxPlayers = 100;

		GameMode->GameSession = GameSession;
		GameMode->FortGameSession = GameSession;

		GameState->WarmupCountdownEndTime = Global->TimeSeconds + Global->BusStartTime;
		GameMode->WarmupCountdownDuration = Global->BusStartTime;

		GameState->WarmupCountdownStartTime = Global->TimeSeconds;
		GameMode->WarmupEarlyCountdownDuration = Global->BusStartTime;

		GameState->GamePhase = EAthenaGamePhase::Warmup;
		GameState->OnRep_GamePhase(EAthenaGamePhase::Aircraft);
	}

	if (!bIsNetReady)
	{
		bIsNetReady = true;
		Util->LOG("FortGameModeAthena::ReadyToStartMatch", std::format("bIsNetReady={}", bIsNetReady).c_str());

		FName GameNetDriver = UKismetStringLibrary::Conv_StringToName(L"GameNetDriver");
		UNetDriver* Driver = Function->CreateNetDriver(UEngine::GetEngine(), UWorld::GetWorld(), GameNetDriver);

		Driver->World = UWorld::GetWorld();
		Driver->NetDriverName = GameNetDriver;
		UWorld::GetWorld()->NetDriver = Driver;

		FString Error;

		FURL URL;
		URL.Port = Global->BeaconPort;

		if (!Function->InitListen(Driver, UWorld::GetWorld(), URL, true, Error))
			Util->LOG("FortGameModeAthena::ReadyToStartMatch", "Failed to listen host");

		Function->SetWorld(Driver, UWorld::GetWorld());

		for (FLevelCollection& LevelCollection : UWorld::GetWorld()->LevelCollections)
			LevelCollection.NetDriver = Driver;

		Util->LOG("FortGameModeAthena::ReadyToStartMatch", std::format("IpNetDriver listening on port {}!", URL.Port));
		SetConsoleTitleW(std::format(L"Atlas | IpNetDriver listening on port {}!", URL.Port).c_str());

	}

	return ReadyToStartMatchOG(GameMode);
}

APawn* FortGameModeAthena::SpawnDefaultPawnFor(AGameModeBase* GameMode, AController* NewPlayer, AActor* StartSpot)
{
	//Util->LOG("FortGameModeAthena::SpawnDefaultPawnFor", std::format("{} Joined!", NewPlayer->PlayerState->GetPlayerName().ToString()).c_str());
	return GameMode->SpawnDefaultPawnAtTransform(NewPlayer, StartSpot->GetTransform());
}

void FortGameModeAthena::HandleStartingNewPlayer(AGameModeBase* GameMode, AAthena_PlayerController_C* NewPlayer)
{
	Util->LOG("FortGameModeAthena::HandleStartingNewPlayer", std::format("{} Joined!", NewPlayer->PlayerState->GetPlayerName().ToString()).c_str());

	APlayerPawn_Athena_C* NewPawn = Util->SpawnActor<APlayerPawn_Athena_C>(NewPlayer->K2_GetActorLocation(), NewPlayer->K2_GetActorRotation());
	AFortPlayerStateAthena* PlayerState = reinterpret_cast<AFortPlayerStateAthena*>(NewPlayer->PlayerState);

	NewPawn->Controller = NewPlayer;
	NewPawn->OnRep_Controller();

	NewPlayer->Pawn = NewPawn;
	NewPlayer->OnRep_Pawn();

	NewPawn->Owner = NewPlayer;
	NewPawn->OnRep_Owner();

	NewPlayer->Possess(NewPawn);

	NewPlayer->bHasClientFinishedLoading = true;
	NewPlayer->bHasServerFinishedLoading = true;
	NewPlayer->OnRep_bHasServerFinishedLoading();

	PlayerState->bHasFinishedLoading = true;
	PlayerState->bHasStartedPlaying = true;
	PlayerState->OnRep_bHasStartedPlaying();

	NewPlayer->ForceNetUpdate();
	PlayerState->ForceNetUpdate();
	NewPawn->ForceNetUpdate();

	NewPlayer->WorldInventory = Util->SpawnActor<AFortInventory>();
	NewPlayer->WorldInventory->SetOwner(NewPlayer);
	NewPlayer->QuickBars = Util->SpawnActor<AFortQuickBars>();
	NewPlayer->QuickBars->SetOwner(NewPlayer);
}
