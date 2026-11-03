#include "TargetHighlightComponent.h"
#include "GameFramework/Character.h"
#include "Engine/SkinnedAsset.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

TArray<TWeakObjectPtr<UTargetHighlightComponent>> UTargetHighlightComponent::AllTargets;

UTargetHighlightComponent::UTargetHighlightComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;	// 피격 발광이 빠지는 동안만

	// 보스 카운터 발광과 같은 오버레이 머티리얼 ("Base Color" 알파 = 불투명도)
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FlashMat(
		TEXT("/Game/Free_Magic/Demo/LevelPrototyping/Materials/M_MirrorLaser.M_MirrorLaser"));
	if (FlashMat.Succeeded())
	{
		HitFlashMaterial = FlashMat.Object;
	}
}

void UTargetHighlightComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (ACharacter* Char = Cast<ACharacter>(Owner))
	{
		TargetMesh = Char->GetMesh();
	}
	else if (Owner)
	{
		// UWidgetComponent(HP바)도 UMeshComponent라 스켈레탈 → 스태틱 순으로 콕 집어 찾는다
		TargetMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
		if (!TargetMesh.IsValid()) TargetMesh = Owner->FindComponentByClass<UStaticMeshComponent>();
	}

	// 커서 판정을 메시 모양 그대로 하려면 메시에 쿼리 충돌이 있어야 한다 — 꺼져 있으면 QueryOnly + 모든 채널 무시로 켬
	// (채널 트레이스·오버랩엔 안 걸리고 LineTraceComponent만 닿음 → 게임플레이 영향 없음)
	if (UMeshComponent* Mesh = TargetMesh.Get())
	{
		const USkeletalMeshComponent* Skel = Cast<USkeletalMeshComponent>(Mesh);
		bTraceBones = Skel && !Skel->GetPhysicsAsset();
		if (!bTraceBones && !Mesh->IsQueryCollisionEnabled())
		{
			Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
			Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		}
	}

	AllTargets.RemoveAll([](const TWeakObjectPtr<UTargetHighlightComponent>& P) { return !P.IsValid(); });
	AllTargets.AddUnique(this);
}

void UTargetHighlightComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AllTargets.RemoveAll([this](const TWeakObjectPtr<UTargetHighlightComponent>& P) { return !P.IsValid() || P.Get() == this; });
	Super::EndPlay(EndPlayReason);
}

void UTargetHighlightComponent::SetHovered(bool bInHovered)
{
	if (bHovered == bInHovered) return;
	bHovered = bInHovered;
	if (UMeshComponent* Mesh = TargetMesh.Get())
	{
		Mesh->SetRenderCustomDepth(bHovered);
		Mesh->SetCustomDepthStencilValue(bHovered ? OutlineStencilValue : 0);
	}
}

bool UTargetHighlightComponent::TraceCursorRay(const FVector& Start, const FVector& End, float& OutDistance) const
{
	UMeshComponent* Mesh = TargetMesh.Get();
	const AActor* Owner = GetOwner();
	// 사라진(숨김·충돌 끔) 보스는 대상이 아님
	if (!Mesh || !Owner || Owner->IsHidden() || !Owner->GetActorEnableCollision() || !Mesh->IsVisible()) return false;

	if (bTraceBones)
	{
		return TraceBones(Start, End, OutDistance);
	}

	// 복합 충돌 = 렌더 삼각형 — 화면에 보이는 모양 그대로
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TargetHighlightCursor), true);
	if (Mesh->LineTraceComponent(Hit, Start, End, Params))
	{
		OutDistance = Hit.Distance;
		return true;
	}
	return false;
}

bool UTargetHighlightComponent::TraceBones(const FVector& Start, const FVector& End, float& OutDistance) const
{
	const USkeletalMeshComponent* Skel = Cast<USkeletalMeshComponent>(TargetMesh.Get());
	const USkinnedAsset* Asset = Skel ? Skel->GetSkinnedAsset() : nullptr;
	if (!Asset) return false;

	const TArray<FTransform>& Bones = Skel->GetComponentSpaceTransforms();
	const FReferenceSkeleton& RefSkel = Asset->GetRefSkeleton();
	const FTransform& ToWorld = Skel->GetComponentTransform();
	const float Radius = CursorBoneRadius * ToWorld.GetMaximumAxisScale();

	// 부모 → 자식 뼈마디마다 광선과의 최단 거리 — 몸 굵기 안이면 맞음, 그중 카메라에 가장 가까운 것
	bool bHit = false;
	float Best = TNumericLimits<float>::Max();
	for (int32 i = 1; i < Bones.Num(); ++i)
	{
		const int32 Parent = RefSkel.GetParentIndex(i);
		if (Parent <= 0 || !Bones.IsValidIndex(Parent)) continue;	// 루트(발밑)에서 나오는 마디는 바닥 쪽이라 뺌
		const FVector A = ToWorld.TransformPosition(Bones[Parent].GetLocation());
		const FVector B = ToWorld.TransformPosition(Bones[i].GetLocation());
		FVector OnRay, OnBone;
		FMath::SegmentDistToSegmentSafe(Start, End, A, B, OnRay, OnBone);
		if (FVector::DistSquared(OnRay, OnBone) <= Radius * Radius)
		{
			const float Along = FVector::Dist(Start, OnRay);
			if (Along < Best)
			{
				Best = Along;
				bHit = true;
			}
		}
	}
	if (bHit) OutDistance = Best;
	return bHit;
}

void UTargetHighlightComponent::PlayHitFlash()
{
	UMeshComponent* Mesh = TargetMesh.Get();
	if (!Mesh || !HitFlashMaterial) return;

	// 다른 오버레이(카운터 발광 등)가 걸려 있으면 그쪽이 우선 — 덮었다 떼면 그 발광이 꺼져 버림
	UMaterialInterface* Current = Mesh->GetOverlayMaterial();
	if (!HitFlashMID)
	{
		HitFlashMID = UMaterialInstanceDynamic::Create(HitFlashMaterial, this);
	}
	if (!HitFlashMID || (Current && Current != HitFlashMID)) return;

	HitFlashMID->SetVectorParameterValue(HitFlashColorParameterName, HitFlashColor);
	Mesh->SetOverlayMaterial(HitFlashMID);
	HitFlashRemaining = HitFlashDuration;
	SetComponentTickEnabled(true);
}

void UTargetHighlightComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	HitFlashRemaining -= DeltaTime;
	if (HitFlashRemaining <= 0.f)
	{
		EndHitFlash();
		return;
	}
	if (HitFlashMID)
	{
		FLinearColor Color = HitFlashColor;
		Color.A *= HitFlashRemaining / FMath::Max(0.01f, HitFlashDuration);
		HitFlashMID->SetVectorParameterValue(HitFlashColorParameterName, Color);
	}
}

void UTargetHighlightComponent::EndHitFlash()
{
	HitFlashRemaining = 0.f;
	SetComponentTickEnabled(false);
	UMeshComponent* Mesh = TargetMesh.Get();
	// 그 사이 다른 오버레이로 바뀌었으면 그대로 둔다
	if (Mesh && Mesh->GetOverlayMaterial() == HitFlashMID)
	{
		Mesh->SetOverlayMaterial(nullptr);
	}
}
