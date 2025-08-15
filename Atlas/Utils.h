#pragma once
#include "framework.h"

class Utils
{
private:
	inline static const std::string FileName = "Atlas.txt";
public:
	void InitConsole();
	void InitLogger();
	void LOG(const std::string& Category, const std::string& Message, const std::string& Info = "Info");
public:

	FQuat RotatorToQuat(FRotator Rotation)
	{
		FQuat Quat{};
		const float DEG_TO_RAD = 3.14159f / 180.0f;
		const float DIVIDE_BY_2 = DEG_TO_RAD / 2.0f;

		float SP = sin(Rotation.Pitch * DIVIDE_BY_2);
		float CP = cos(Rotation.Pitch * DIVIDE_BY_2);
		float SY = sin(Rotation.Yaw * DIVIDE_BY_2);
		float CY = cos(Rotation.Yaw * DIVIDE_BY_2);
		float SR = sin(Rotation.Roll * DIVIDE_BY_2);
		float CR = cos(Rotation.Roll * DIVIDE_BY_2);

		Quat.X = CR * SP * SY - SR * CP * CY;
		Quat.Y = -CR * SP * CY - SR * CP * SY;
		Quat.Z = CR * CP * SY - SR * SP * CY;
		Quat.W = CR * CP * CY + SR * SP * SY;

		return Quat;
	}

	template<typename T = AActor>
	T* SpawnActor(FVector Location = { 0,0,0 }, FRotator Rotation = { 0,0,0 }, UClass* StaticClass = T::StaticClass(), AActor* Owner = nullptr)
	{
		FTransform Transform{};

		Transform.Scale3D = { 1,1,1 };
		Transform.Translation = Location;
		Transform.Rotation = RotatorToQuat(Rotation);

		AActor* Actor = UGameplayStatics::BeginDeferredActorSpawnFromClass(UWorld::GetWorld(), StaticClass, Transform, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn, Owner);
		if (Actor) return (T*)UGameplayStatics::FinishSpawningActor(Actor, Transform);
		
		LOG("SpawnActor", "Nullptr");
		return nullptr;
	}
};