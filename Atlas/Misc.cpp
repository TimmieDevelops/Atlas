#include "Misc.h"

inline Globals* Global = new Globals();

void Misc::Hook()
{
	VirtualHook<int>(IsTrue).Hook(OFFSET_NoReservation);
	VirtualHook<int>(IsFalse).Hook(0x601FB0); // ChangeGameSessionId
	VirtualHook<ENetMode>(InternalGetNetMode).Hook(OFFSET_WorldInternalGetNetMode);
	VirtualHook<ENetMode>(InternalGetNetMode).Hook(OFFSET_ActorInternalGetNetMode);
	VirtualHook<float>(GetMaxTickRate).Hook(OFFSET_GetMaxTickRate);
}

int Misc::IsTrue()
{
	return 1;
}

int Misc::IsFalse()
{
	return 0;
}

ENetMode Misc::InternalGetNetMode()
{
	return Global->NetMode;
}

float Misc::GetMaxTickRate()
{
	return Global->MaxTickRate;
}
