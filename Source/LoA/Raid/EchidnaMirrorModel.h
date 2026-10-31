#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"

class UMaterialInterface;

/**
 * 에키드나 거울 모델(SM_EchidnaMirror — 원본 Mirror_v1.psk를 아마추어 떼고 정적 메시로 변환) 공용 치수·배치.
 * 피벗 = 바닥 가운데, 정면 = 로컬 +X(앞쪽 장식이 +X로 15cm 튀어나옴, 뒷면도 장식 있는 양면), 폭 = Y.
 * 슬롯 0·1 = 프레임(원본 텍스처 MI), 2·3 = 유리(2 = 앞면, 3 = 깨지는 조각 — 둘 다 x≈0 평면). 유리 색은 슬롯 2·3에만 입힌다.
 * 4거울·거울잇기·거울 카운터 줄·거울 섞기가 같이 쓴다.
 */
namespace EchidnaMirrorModel
{
	inline const TCHAR* MeshPath = TEXT("/Game/LostArk/Raid/EchidnaModling/other/Mirror/SM_EchidnaMirror.SM_EchidnaMirror");

	// 원본 크기 (cm)
	constexpr float FrameWidth = 229.8f;
	constexpr float FrameHeight = 301.1f;
	constexpr float GlassWidth = 138.8f;
	constexpr float GlassHeight = 176.9f;
	constexpr float GlassCenterZ = 137.15f;

	constexpr int32 GlassSlotFront = 2;
	constexpr int32 GlassSlotShards = 3;

	/** 프레임 전체가 Width×Height 상자 안에 들어가는 균일 스케일 */
	inline float FitScale(float Width, float Height)
	{
		return FMath::Max(0.01f, FMath::Min(Width / FrameWidth, Height / FrameHeight));
	}

	/** 부모 원점(바닥)에 똑바로 세우고 Width×Height 상자에 맞춘다. 적용한 스케일 반환 */
	inline float Place(UStaticMeshComponent* Mesh, float Width, float Height, float BaseZ = 0.f)
	{
		const float Scale = FitScale(Width, Height);
		if (Mesh)
		{
			Mesh->SetRelativeLocationAndRotation(FVector(0.f, 0.f, BaseZ), FRotator::ZeroRotator);
			Mesh->SetRelativeScale3D(FVector(Scale));
		}
		return Scale;
	}

	inline void SetGlassMaterial(UStaticMeshComponent* Mesh, UMaterialInterface* Material)
	{
		if (!Mesh || !Material) return;
		Mesh->SetMaterial(GlassSlotFront, Material);
		Mesh->SetMaterial(GlassSlotShards, Material);
	}

	/** 유리 앞면 슬롯의 머티리얼 (색 MID를 다시 꺼낼 때) */
	inline UMaterialInterface* GetGlassMaterial(const UStaticMeshComponent* Mesh)
	{
		return Mesh ? Mesh->GetMaterial(GlassSlotFront) : nullptr;
	}

	/** 유리 타원 중심의 월드 위치 */
	inline FVector GetGlassCenter(const UStaticMeshComponent* Mesh)
	{
		return Mesh ? Mesh->GetComponentTransform().TransformPosition(FVector(0.f, 0.f, GlassCenterZ)) : FVector::ZeroVector;
	}
}
