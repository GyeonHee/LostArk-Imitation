#include "Raid/PatternVFXUtil.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

namespace
{
	// 예상 개수(면적 / 간격²)가 상한을 넘으면 간격을 sqrt 비율로 넓힌다
	float PatternVFXAdjustSpacing(float Spacing, float Area, int32 MaxCount)
	{
		Spacing = FMath::Max(Spacing, 30.f);
		const float Estimated = Area / (Spacing * Spacing);
		if (MaxCount > 0 && Estimated > MaxCount)
		{
			Spacing *= FMath::Sqrt(Estimated / MaxCount);
		}
		return Spacing;
	}

	void PatternVFXSpawnOne(UObject* WorldContext, UNiagaraSystem* System, const FTransform& Origin,
		const FVector& LocalPos, float LocalYawDeg, float Scale)
	{
		const FVector WorldPos = Origin.TransformPosition(LocalPos);
		const FRotator WorldRot = Origin.TransformRotation(FRotator(0.f, LocalYawDeg, 0.f).Quaternion()).Rotator();
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(WorldContext, System, WorldPos, WorldRot, FVector(Scale), true);
	}
}

namespace PatternVFX
{
	int32 SpawnFanArea(UObject* WorldContext, UNiagaraSystem* System, const FTransform& Origin,
		float InnerRadius, float OuterRadius, float HalfAngleDeg, float Spacing, float Scale, int32 MaxCount)
	{
		if (!WorldContext || !System) return 0;

		InnerRadius = FMath::Max(InnerRadius, 0.f);
		const float Width = OuterRadius - InnerRadius;
		if (Width <= KINDA_SMALL_NUMBER) return 0;

		const float HalfAngleRad = FMath::DegreesToRadians(FMath::Clamp(HalfAngleDeg, 1.f, 180.f));
		const float Area = HalfAngleRad * (OuterRadius * OuterRadius - InnerRadius * InnerRadius);
		Spacing = PatternVFXAdjustSpacing(Spacing, Area, MaxCount);

		// 반지름 방향으로 줄을 나누고, 각 줄은 호 길이에 비례한 개수로 나눈다
		const int32 Rows = FMath::Max(1, FMath::CeilToInt(Width / Spacing));
		int32 Count = 0;
		for (int32 Row = 0; Row < Rows; Row++)
		{
			const float R = InnerRadius + Width * ((Row + 0.5f) / Rows);
			const int32 Cols = FMath::Max(1, FMath::CeilToInt(2.f * HalfAngleRad * R / Spacing));
			for (int32 Col = 0; Col < Cols; Col++)
			{
				const float A = FMath::Lerp(-HalfAngleRad, HalfAngleRad, (Col + 0.5f) / Cols);
				PatternVFXSpawnOne(WorldContext, System, Origin,
					FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 5.f), FMath::RadiansToDegrees(A), Scale);
				Count++;
			}
		}
		return Count;
	}

	int32 SpawnRectArea(UObject* WorldContext, UNiagaraSystem* System, const FTransform& Origin,
		float XStart, float XEnd, float HalfWidth, float Spacing, float Scale, int32 MaxCount)
	{
		if (!WorldContext || !System) return 0;

		const float Length = XEnd - XStart;
		const float Width = HalfWidth * 2.f;
		if (Length <= KINDA_SMALL_NUMBER || Width <= KINDA_SMALL_NUMBER) return 0;

		Spacing = PatternVFXAdjustSpacing(Spacing, Length * Width, MaxCount);

		const int32 Cols = FMath::Max(1, FMath::CeilToInt(Length / Spacing));
		const int32 Rows = FMath::Max(1, FMath::CeilToInt(Width / Spacing));
		int32 Count = 0;
		for (int32 Col = 0; Col < Cols; Col++)
		{
			const float X = XStart + Length * ((Col + 0.5f) / Cols);
			for (int32 Row = 0; Row < Rows; Row++)
			{
				const float Y = -HalfWidth + Width * ((Row + 0.5f) / Rows);
				PatternVFXSpawnOne(WorldContext, System, Origin, FVector(X, Y, 5.f), 0.f, Scale);
				Count++;
			}
		}
		return Count;
	}
}
