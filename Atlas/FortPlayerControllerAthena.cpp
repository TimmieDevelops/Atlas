#include "FortPlayerControllerAthena.h"

inline Utils* Util = new Utils();
inline Func* Function = new Func();

void FortPlayerControllerAthena::Hook()
{
	VirtualHook<void>(GetPlayerViewPoint, reinterpret_cast<void**>(&GetPlayerViewPointOG)).Hook(OFFSET_GetPlayerViewPoint);
	VirtualHook<AFortPlayerControllerAthena>(ServerAcknowledgePossession).VFT(258);
	VirtualHook<AFortPlayerControllerAthena>(ServerLoadingScreenDropped).VFT(568);
}

void FortPlayerControllerAthena::GetPlayerViewPoint(const APlayerController* PC, FVector& out_Location, FRotator& out_Rotation)
{
	if (PC->StateName == UKismetStringLibrary::Conv_StringToName(L"Spectating") && PC->HasAuthority() && !PC->IsLocalController())
	{
		out_Location = PC->LastSpectatorSyncLocation;
		out_Rotation = PC->LastSpectatorSyncRotation;
	}
	else if (AActor* TheViewTarget = PC->GetViewTarget())
	{
		if (TheViewTarget)
		{
			out_Location = TheViewTarget->K2_GetActorLocation();
			out_Rotation = TheViewTarget->K2_GetActorRotation();
		}
		else
		{
			GetPlayerViewPointOG(PC, out_Location, out_Rotation);
		}
	}

	return GetPlayerViewPointOG(PC, out_Location, out_Rotation);
}

void FortPlayerControllerAthena::ServerAcknowledgePossession(APlayerController* PC, APawn* P)
{
	Util->LOG("FortPlayerControllerAthena::ServerAcknowledgePossession", "Function Called!");
	PC->AcknowledgedPawn = P;
}

void FortPlayerControllerAthena::ServerLoadingScreenDropped(AFortPlayerController* PC)
{
	Util->LOG("FortPlayerControllerAthena::ServerLoadingScreenDropped", "Function Called!");
	AFortPlayerState* PlayerState = (AFortPlayerState*)PC->PlayerState;
	Function->ApplyCharacterCustomization(PlayerState, PC->MyFortPawn);
}
