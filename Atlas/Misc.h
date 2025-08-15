#pragma once
#include "framework.h"
#include "VirtualHook.h"
#include "Globals.h"

namespace Misc
{
	void Hook();
	int IsTrue();
	int IsFalse();
	ENetMode InternalGetNetMode();
	float GetMaxTickRate();
}