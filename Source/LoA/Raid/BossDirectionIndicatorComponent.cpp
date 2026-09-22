#include "BossDirectionIndicatorComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "LoA.h"

UBossDirectionIndicatorComponent::UBossDirectionIndicatorComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;

	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCastShadow(false);
	bUseComplexAsSimpleCollision = true;

	// 부채꼴 장판·줄기와 같은 머티리얼 컨벤션 — Translucent+Unlit에 "Base Color" Vector Parameter가 있어야
	// 색/불투명도가 실제로 먹는다
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultMatFinder(
		TEXT("/Game/Free_Magic/Demo/LevelPrototyping/Materials/M_MirrorLaser.M_MirrorLaser"));
	if (DefaultMatFinder.Succeeded())
	{
		BaseMaterial = DefaultMatFinder.Object;
	}
}

void UBossDirectionIndicatorComponent::OnRegister()
{
	Super::OnRegister();

	ApplyFeetOffset();
	RebuildIndicator();
}

void UBossDirectionIndicatorComponent::BeginPlay()
{
	Super::BeginPlay();

	ApplyFeetOffset();
	RebuildIndicator();
}

#if WITH_EDITOR
void UBossDirectionIndicatorComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ApplyFeetOffset();
	RebuildIndicator();
}
#endif

void UBossDirectionIndicatorComponent::ApplyFeetOffset()
{
	if (!bSnapToOwnerFeet) return;

	// 오너의 GetActorLocation은 캡슐 "중심"이라 그대로 두면 표시가 보스 허리 높이에 뜬다.
	//
	// **반드시 Unscaled를 써야 한다.** 이 컴포넌트는 캡슐의 자식이라 상대 오프셋에 부모(캡슐)의 스케일이
	// 다시 곱해진다. 에키드나 캡슐은 스케일이 2배라 Scaled(176)를 넣으면 실제로는 -352가 되어
	// 바닥 한참 아래에 묻혀 안 보였다. Unscaled(88) × 부모 스케일 2 = -176이 정확한 발밑 위치.
	if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		if (UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent())
		{
			SetRelativeLocation(FVector(0.f, 0.f, -Capsule->GetUnscaledCapsuleHalfHeight()));
		}
	}
}

void UBossDirectionIndicatorComponent::RebuildIndicator()
{
	ClearAllMeshSections();

	// 로컬 +X = 보스 정면. 가운데가 뾰족하게 튀어나온 호
	BuildArcSection(0, 0.f, FrontArcAngle, FrontSpikeLength, FrontColor);

	// 로컬 -X = 보스 후방. 스파이크 없이 매끈한 호
	BuildArcSection(1, 180.f, BackArcAngle, 0.f, BackColor);

	UE_LOG(LogLoA, Verbose, TEXT("[BossDirectionIndicator] Rebuild — Owner=%s Sections=%d RelLoc=%s Material=%s"),
		*GetNameSafe(GetOwner()), GetNumSections(), *GetRelativeLocation().ToString(), *GetNameSafe(BaseMaterial));
}

void UBossDirectionIndicatorComponent::BuildArcSection(int32 SectionIndex, float CenterYawDeg,
	float ArcAngleDeg, float SpikeLength, const FLinearColor& Color)
{
	if (ArcAngleDeg <= 0.f || ArcSegments < 4)
	{
		return;
	}

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;

	const float StartYaw = CenterYawDeg - ArcAngleDeg * 0.5f;

	for (int32 i = 0; i <= ArcSegments; i++)
	{
		const float Alpha = static_cast<float>(i) / static_cast<float>(ArcSegments);
		const float Yaw = StartYaw + ArcAngleDeg * Alpha;
		const float Rad = FMath::DegreesToRadians(Yaw);
		const FVector Dir(FMath::Cos(Rad), FMath::Sin(Rad), 0.f);

		// 스파이크를 별도 삼각형으로 붙이지 않고 바깥 반지름을 각도의 함수로 둔다 —
		// 중심에서 SpikeHalfAngle만큼 떨어지면 0이 되는 선형 보간이라 호와 자연스럽게 이어진다
		float Spike = 0.f;
		if (SpikeLength > 0.f && SpikeHalfAngle > 0.f)
		{
			const float AngleFromCenter = FMath::Abs(Yaw - CenterYawDeg);
			Spike = SpikeLength * FMath::Max(0.f, 1.f - AngleFromCenter / SpikeHalfAngle);
		}

		const FVector ZOffset(0.f, 0.f, IndicatorHeight);
		Vertices.Add(Dir * InnerRadius + ZOffset);
		Vertices.Add(Dir * (OuterRadius + Spike) + ZOffset);

		Normals.Add(FVector::UpVector);
		Normals.Add(FVector::UpVector);
		UVs.Add(FVector2D(Alpha, 0.f));
		UVs.Add(FVector2D(Alpha, 1.f));
		Tangents.Add(FProcMeshTangent(1.f, 0.f, 0.f));
		Tangents.Add(FProcMeshTangent(1.f, 0.f, 0.f));
	}

	for (int32 i = 0; i < ArcSegments; i++)
	{
		const int32 Inner0 = i * 2;
		const int32 Outer0 = i * 2 + 1;
		const int32 Inner1 = Inner0 + 2;
		const int32 Outer1 = Outer0 + 2;

		// 탑다운이라 윗면만 있어도 되지만, 양쪽 감김을 다 넣어두면 카메라 각도와 무관하게 항상 보인다
		// (부채꼴 장판의 AddFanQuad와 동일한 방침)
		Triangles.Add(Inner0); Triangles.Add(Outer0); Triangles.Add(Outer1);
		Triangles.Add(Inner0); Triangles.Add(Outer1); Triangles.Add(Inner1);

		Triangles.Add(Inner0); Triangles.Add(Outer1); Triangles.Add(Outer0);
		Triangles.Add(Inner0); Triangles.Add(Inner1); Triangles.Add(Outer1);
	}

	CreateMeshSection(SectionIndex, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, false);

	if (BaseMaterial)
	{
		UMaterialInstanceDynamic* MID = CreateAndSetMaterialInstanceDynamicFromMaterial(SectionIndex, BaseMaterial);
		if (MID)
		{
			MID->SetVectorParameterValue(ColorParameterName,
				FLinearColor(Color.R, Color.G, Color.B, IndicatorOpacity));
		}
	}
}
