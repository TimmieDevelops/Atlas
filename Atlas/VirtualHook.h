#pragma once
#include "framework.h"

template<typename T>
class VirtualHook
{
public:
	void* Detour;
	void** Original;

	VirtualHook(void* InDetour, void** InOriginal = nullptr)
		: Detour(InDetour), Original(InOriginal) {}

	void VFT(int Idx)
	{
		void** VTable = T::GetDefaultObj()->VTable;
		if (Original) *Original = VTable[Idx];
		DWORD dwOldProtect{};
		VirtualProtect(&VTable[Idx], sizeof(void*), PAGE_EXECUTE_READWRITE, &dwOldProtect);
		VTable[Idx] = Detour;
		VirtualProtect(&VTable[Idx], sizeof(void*), dwOldProtect, &dwOldProtect);
	}

	void Hook(int Idx)
	{
		void* Address = reinterpret_cast<void*>(InSDKUtils::GetImageBase() + Idx);
		MH_CreateHook(Address, Detour, Original);
		MH_EnableHook(Address);
	}
};