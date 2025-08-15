#pragma once
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>
#include <iostream>
#include "Includes/MinHook/MinHook.h"
#pragma comment(lib, "Includes/MinHook/minhook.x64.lib")
#include "Includes/SDK/CppSDK/SDK.hpp"
using namespace SDK;
#include "Includes/Tim's Finder/Address.h"
#include "Includes/Tim's Finder/Func.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <filesystem>
#include <sstream>
#include <fstream>
#include <unordered_map>

/** Types of net modes that we know about - synced with EngineBaseTypes.h */
enum class ENetMode : uint8
{
	Standalone,
	DedicatedServer,
	ListenServer,
	Client,

	MAX,
};
