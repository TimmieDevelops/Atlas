// dllmain.cpp : Defines the entry point for the DLL application.
#include "framework.h"
#include "Utils.h"
#include "FortGameModeAthena.h"
#include "Misc.h"
#include "NetworkDriver.h"
#include "FortPlayerControllerAthena.h"

inline Utils* Util = new Utils();
inline Func* Function = new Func();
inline FortGameModeAthena* GameMode = new FortGameModeAthena();
inline NetDriver* Driver = new NetDriver();
inline FortPlayerControllerAthena* PC = new FortPlayerControllerAthena();

DWORD MainThread(HMODULE Module)
{
    MH_Initialize();

    Util->InitConsole();
    Util->InitLogger();

    GameMode->Hook();
    Misc::Hook();
    Driver->Hook();
    PC->Hook();

    Util->LOG("MapInfo", "Initializing Atlas hooks & Loading MapInfo");

    Sleep(5000); // Sleeps for 5 seconds (hopefully no crashs)

    *(bool*)(Function->ImageBase + OFFSET_GIsClient) = false;
    *(bool*)(Function->ImageBase + OFFSET_GIsServer) = true;

    UGameplayStatics::OpenLevel(UWorld::GetWorld(), UKismetStringLibrary::Conv_StringToName(L"Athena_Terrain"), true, FString());
    UWorld::GetWorld()->OwningGameInstance->LocalPlayers.Free();

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        CreateThread(0, 0, (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, 0);
        break;
    }

    return TRUE;
}