#include "Raid/EchidnaMirrorWallActor.h"
#include "Raid/EchidnaBoss.h"
#include "LoACharacter.h"
#include "LoA.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"

namespace
{
	// 앞/뒤 양쪽 감김 (다른 PMC 액터들과 같은 방식)
	void AddFireQuad(TArray<FVector>& Verts, TArray<int32>& Tris,
		const FVector& A, const FVector& B, const FVector& C, const FVector& D)
	{
		const int32 Base = Verts.Num();
		Verts.Append({ A, B, C, D });
		Tris.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
		Tris.Append({ Base, Base + 2, Base + 1, Base, Base + 3, Base + 2 });
	}

	float GetCapsuleRadius(const ACharacter* Character)
	{
		const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
		return Capsule ? Capsule->GetScaledCapsuleRadius() : 0.f;
	}

	// 불길은 지면에 딱 붙이면 타일과 Z-파이팅이 나서 살짝 띄운다
	constexpr float FireHeight = 4.f;
}

AEchidnaMirrorWallActor::AEchidnaMirrorWallActor()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	RowRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RowRoot"));
	RowRoot->SetupAttachment(Root);

	FireMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("FireMesh"));
	FireMesh->SetupAttachment(Root);
	FireMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FireMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderFinder.Succeeded())
	{
		MirrorMesh = CylinderFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
		TEXT("/Game/Free_Magic/Demo/LevelPrototyping/Materials/M_MirrorLaser.M_MirrorLaser"));
	if (MatFinder.Succeeded())
	{
		BaseMaterial = MatFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PortraitMatFinder(
		TEXT("/Game/LostArk/Raid/Echidna/Pattern/M_EchidnaMirrorPortrait.M_EchidnaMirrorPortrait"));
	if (PortraitMatFinder.Succeeded())
	{
		PortraitMaterial = PortraitMatFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UTexture2D> NormalPortraitFinder(
		TEXT("/Game/LostArk/Raid/EchidnaModling/other/Mirror/materials/textures/fx/fx_l_mirror_sden_01_cl.fx_l_mirror_sden_01_cl"));
	if (NormalPortraitFinder.Succeeded())
	{
		NormalPortraitTexture = NormalPortraitFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UTexture2D> CounterPortraitFinder(
		TEXT("/Game/LostArk/Raid/EchidnaModling/other/Mirror/materials/textures/fx/fx_l_mirror_sden_02_cl.fx_l_mirror_sden_02_cl"));
	if (CounterPortraitFinder.Succeeded())
	{
		CounterPortraitTexture = CounterPortraitFinder.Object;
	}
}

void AEchidnaMirrorWallActor::BeginPlay()
{
	Super::BeginPlay();

	// 광폭화 규칙 — Tick 기반 진행은 이 값으로 자동 가속
	CustomTimeDilation = AEchidnaBoss::GetEnrageTimeScale(this);

	if (BaseMaterial)
	{
		UMaterialInstanceDynamic* FireMID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		FireMID->SetVectorParameterValue(ColorParameterName, FireColor);
		FireMesh->SetMaterial(0, FireMID);
	}
}

float AEchidnaMirrorWallActor::GetRowHalfWidth() const
{
	return (FMath::Max(1, MirrorCount) - 1) * 0.5f * MirrorSpacing + MirrorWidth * 0.5f;
}

float AEchidnaMirrorWallActor::GetMirrorLocalY(int32 Index) const
{
	return (Index - (FMath::Max(1, MirrorCount) - 1) * 0.5f) * MirrorSpacing;
}

void AEchidnaMirrorWallActor::Activate(AEchidnaBoss* InBoss, float InTravelDistance, float InFireStartDistance, AController* InInstigator, float RowWidth)
{
	Boss = InBoss;
	InstigatorController = InInstigator;
	TravelDistance = FMath::Max(0.f, InTravelDistance);
	FireStartDistance = FMath::Clamp(InFireStartDistance, 0.f, TravelDistance);

	// 양 끝 거울의 바깥 가장자리가 RowWidth 안에 오도록 간격 재계산 (거울 폭보다 좁아지진 않게)
	if (RowWidth > 0.f && MirrorCount > 1)
	{
		MirrorSpacing = FMath::Max(MirrorWidth, (RowWidth - MirrorWidth) / (MirrorCount - 1));
	}

	RowOffset = 0.f;
	PhaseElapsed = 0.f;
	FireTickElapsed = 0.f;
	bCountered = false;
	MirrorHitActors.Reset();
	CounterMirrorIndex = FMath::RandRange(0, FMath::Max(1, MirrorCount) - 1);

	BuildMirrors();
	Phase = EEchidnaMirrorWallPhase::Advancing;
}

void AEchidnaMirrorWallActor::BuildMirrors()
{
	for (USceneComponent* Pivot : MirrorPivots)
	{
		if (Pivot) Pivot->DestroyComponent(true);
	}
	MirrorPivots.Reset();
	MirrorMeshes.Reset();
	PortraitMeshes.Reset();

	for (int32 i = 0; i < FMath::Max(1, MirrorCount); ++i)
	{
		// 받침 — 지면 높이에서 Y축(좌우)으로 돌아 뒤로 쓰러진다
		USceneComponent* Pivot = NewObject<USceneComponent>(this);
		Pivot->SetupAttachment(RowRoot);
		Pivot->SetRelativeLocation(FVector(0.f, GetMirrorLocalY(i), 0.f));
		Pivot->RegisterComponent();

		// 엔진 Cylinder(지름·높이 100, 중심 피벗)를 Pitch 90으로 세우면 원기둥 축이 진행 방향(+X)을 향해
		// "정면을 보는 얇은 타원판"이 된다. Pitch 90 회전에서 로컬 X → 위, 로컬 Z → 진행 방향이므로
		// 스케일은 (높이, 폭, 두께) 순서
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this);
		Mesh->SetupAttachment(Pivot);
		Mesh->SetStaticMesh(MirrorMesh);
		Mesh->SetRelativeLocation(FVector(0.f, 0.f, MirrorHeight * 0.5f));
		Mesh->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
		Mesh->SetRelativeScale3D(FVector(MirrorHeight / 100.f, MirrorWidth / 100.f, MirrorThickness / 100.f));

		// 돌풍 토네이도의 오브젝트 타입 오버랩(AllDynamicObjects)에만 잡히면 된다 — 채널 응답은 전부 무시해서
		// 플레이어를 밀어내거나 마우스 클릭 트레이스(Visibility)를 가로채지 않게 한다
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Mesh->SetCollisionObjectType(ECC_WorldDynamic);
		Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCastShadow(true);

		if (BaseMaterial)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
			MID->SetVectorParameterValue(ColorParameterName, i == CounterMirrorIndex ? CounterMirrorColor : NormalMirrorColor);
			Mesh->SetMaterial(0, MID);
		}

		Mesh->RegisterComponent();

		MirrorPivots.Add(Pivot);
		MirrorMeshes.Add(Mesh);
		PortraitMeshes.Add(CreatePortrait(Pivot, i == CounterMirrorIndex));
	}
}

UProceduralMeshComponent* AEchidnaMirrorWallActor::CreatePortrait(USceneComponent* Pivot, bool bCounterMirror)
{
	UTexture2D* Texture = bCounterMirror ? CounterPortraitTexture.Get() : NormalPortraitTexture.Get();
	if (!Pivot || !PortraitMaterial || !Texture) return nullptr;

	UProceduralMeshComponent* Portrait = NewObject<UProceduralMeshComponent>(this);
	Portrait->SetupAttachment(Pivot);
	Portrait->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Portrait->SetCastShadow(false);

	// 거울(세운 Cylinder)은 받침 기준 X 방향 두께가 가운데 정렬이라 앞면 = +두께/2. 1cm 띄워 Z-파이팅 방지
	const float FrontX = MirrorThickness * 0.5f + 1.f;
	const float CenterZ = MirrorHeight * 0.5f;
	const float RadiusY = MirrorWidth * 0.5f * PortraitInset;
	const float RadiusZ = MirrorHeight * 0.5f * PortraitInset;

	// 타원 팬 — 거울 테두리를 따라 잘려 사각형 모서리가 삐져나오지 않는다.
	// UV는 세로 반지름 하나로 양축을 나눠 텍스처 비율(정사각)을 유지하고 가로는 가운데만 잘라 쓴다.
	// 정면(+X)에서 보는 사람에겐 +Y가 왼쪽이므로 u는 +Y에서 0
	auto ToUV = [RadiusZ](float Y, float DZ)
	{
		return FVector2D(0.5f - Y / (2.f * RadiusZ), 0.5f - DZ / (2.f * RadiusZ));
	};

	constexpr int32 Segments = 40;
	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector2D> UVs;

	Verts.Add(FVector(FrontX, 0.f, CenterZ));
	UVs.Add(ToUV(0.f, 0.f));
	for (int32 s = 0; s <= Segments; ++s)
	{
		const float Angle = 2.f * PI * s / Segments;
		const float Y = FMath::Cos(Angle) * RadiusY;
		const float DZ = FMath::Sin(Angle) * RadiusZ;
		Verts.Add(FVector(FrontX, Y, CenterZ + DZ));
		UVs.Add(ToUV(Y, DZ));
	}
	for (int32 s = 1; s <= Segments; ++s)
	{
		// 양쪽 감김 — PMC 앞면 방향과 무관하게 항상 보이게 (다른 PMC 액터들과 같은 방식)
		Tris.Append({ 0, s, s + 1, 0, s + 1, s });
	}

	Portrait->CreateMeshSection(0, Verts, Tris, TArray<FVector>(), UVs, TArray<FColor>(), TArray<FProcMeshTangent>(), false);

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(PortraitMaterial, this);
	MID->SetTextureParameterValue(PortraitTextureParameterName, Texture);
	MID->SetVectorParameterValue(PortraitTintParameterName, bCounterMirror ? CounterPortraitTint : NormalPortraitTint);
	Portrait->SetMaterial(0, MID);

	Portrait->RegisterComponent();
	return Portrait;
}

void AEchidnaMirrorWallActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	switch (Phase)
	{
	case EEchidnaMirrorWallPhase::Advancing:
		TickAdvancing(DeltaTime);
		break;

	case EEchidnaMirrorWallPhase::Falling:
	{
		PhaseElapsed += DeltaTime;

		// 뒤로 쓰러짐 — Pitch +90이면 받침의 위(+Z)가 뒤(-X)로 눕는다. ease-in으로 점점 빨라지게
		const float Alpha = FallDuration > 0.f ? FMath::Clamp(PhaseElapsed / FallDuration, 0.f, 1.f) : 1.f;
		const float Pitch = 90.f * Alpha * Alpha;
		for (USceneComponent* Pivot : MirrorPivots)
		{
			if (Pivot) Pivot->SetRelativeRotation(FRotator(Pitch, 0.f, 0.f));
		}

		if (PhaseElapsed >= FallDuration + FallenLingerDuration)
		{
			VanishRow();
		}
		break;
	}

	default:
		break;
	}

	if (Phase == EEchidnaMirrorWallPhase::Advancing || Phase == EEchidnaMirrorWallPhase::Falling)
	{
		TickFire(DeltaTime);
	}
}

void AEchidnaMirrorWallActor::TickAdvancing(float DeltaTime)
{
	RowOffset = FMath::Min(RowOffset + MoveSpeed * DeltaTime, TravelDistance);
	RowRoot->SetRelativeLocation(FVector(RowOffset, 0.f, 0.f));
	UpdateFireMesh();

	// 전진하는 거울 줄에 닿으면 1회 데미지 + 진행 방향으로 넉다운
	const FTransform& Xform = GetActorTransform();
	const float HalfWidth = GetRowHalfWidth();
	for (TActorIterator<ALoACharacter> It(GetWorld()); It; ++It)
	{
		ALoACharacter* Character = *It;
		if (MirrorHitActors.Contains(Character)) continue;

		const FVector Local = Xform.InverseTransformPosition(Character->GetActorLocation());
		const float Margin = GetCapsuleRadius(Character);
		if (FMath::Abs(Local.X - RowOffset) <= MirrorHitThickness + Margin && FMath::Abs(Local.Y) <= HalfWidth + Margin)
		{
			MirrorHitActors.Add(Character);
			UGameplayStatics::ApplyDamage(Character, MirrorHitDamage, InstigatorController.Get(), this, UDamageType::StaticClass());
			if (bKnockdownOnMirrorHit)
			{
				// 넉다운은 SourceLocation 반대쪽으로 튕기므로, 캐릭터 바로 뒤(-X) 지점을 넘겨 진행 방향으로 밀려나게
				Character->ApplyKnockdown(Xform.TransformPosition(FVector(Local.X - 300.f, Local.Y, Local.Z)));
			}
		}
	}

	if (RowOffset >= TravelDistance)
	{
		// 카운터 못 치고 반대편 끝(맵 밖)까지 감 — 거울은 벽 너머라 불길과 함께 바로 사라진다
		UE_LOG(LogLoA, Log, TEXT("[MirrorWall] 반대편 도착 — 카운터 실패"));
		StopRow();
		VanishRow();
	}
}

void AEchidnaMirrorWallActor::StopRow()
{
	// 줄 전진 정지 — 거울은 더 이상 카운터 대상이 아니다. 불길은 거울이 사라질 때(VanishRow)까지 유지
	PhaseElapsed = 0.f;
	for (UStaticMeshComponent* Mesh : MirrorMeshes)
	{
		if (Mesh) Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AEchidnaMirrorWallActor::VanishRow()
{
	// 불길은 거울과 수명이 같다 — 거울이 사라지는 순간 같이 꺼진다.
	// 스스로 소멸하지 않는다 — 패턴 Task가 끝날 때 카운터 결과를 센 뒤 파괴한다
	RowRoot->SetVisibility(false, true);
	FireMesh->ClearAllMeshSections();
	Phase = EEchidnaMirrorWallPhase::Done;
}

void AEchidnaMirrorWallActor::TickFire(float DeltaTime)
{
	FireTickElapsed += DeltaTime;
	if (FireTickInterval <= 0.f || FireTickElapsed < FireTickInterval) return;
	FireTickElapsed -= FireTickInterval;

	if (RowOffset <= FireStartDistance) return;

	// 불길 = 시작선(FireStartDistance) ~ 줄이 지나간 곳, 줄 전체 폭.
	// 여러 줄의 불길이 겹쳐도 캐릭터 쪽에서 한 틱분만 받는다(TryConsumeTickDamage). 간격은 실제 시간 기준이라 광폭화 배율로 나눈다
	const float RealTickInterval = FireTickInterval / FMath::Max(CustomTimeDilation, KINDA_SMALL_NUMBER);
	const FTransform& Xform = GetActorTransform();
	const float HalfWidth = GetRowHalfWidth();
	for (TActorIterator<ALoACharacter> It(GetWorld()); It; ++It)
	{
		const FVector Local = Xform.InverseTransformPosition(It->GetActorLocation());
		const float Margin = GetCapsuleRadius(*It);
		if (Local.X >= FireStartDistance - Margin && Local.X <= RowOffset - MirrorHitThickness && FMath::Abs(Local.Y) <= HalfWidth + Margin
			&& It->TryConsumeTickDamage(TEXT("MirrorWallFire"), RealTickInterval))
		{
			UGameplayStatics::ApplyDamage(*It, FireTickDamage, InstigatorController.Get(), this, UDamageType::StaticClass());
		}
	}
}

void AEchidnaMirrorWallActor::UpdateFireMesh()
{
	if (RowOffset <= FireStartDistance)
	{
		FireMesh->ClearAllMeshSections();
		return;
	}

	const float HalfWidth = GetRowHalfWidth();
	TArray<FVector> Verts;
	TArray<int32> Tris;
	AddFireQuad(Verts, Tris,
		FVector(FireStartDistance, -HalfWidth, FireHeight),
		FVector(RowOffset, -HalfWidth, FireHeight),
		FVector(RowOffset, HalfWidth, FireHeight),
		FVector(FireStartDistance, HalfWidth, FireHeight));

	FireMesh->CreateMeshSection(0, Verts, Tris, TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), false);
}

bool AEchidnaMirrorWallActor::TryCounterHit(AActor* Attacker, UPrimitiveComponent* HitComponent)
{
	if (Phase != EEchidnaMirrorWallPhase::Advancing || !IsValid(Attacker)) return false;
	if (!MirrorMeshes.IsValidIndex(CounterMirrorIndex) || HitComponent != MirrorMeshes[CounterMirrorIndex]) return false;

	// 정면 판정 — 카운터 거울 위치에서 시전자까지의 방향이 진행 방향(+X)과 CounterHalfAngle 이내
	const FVector AttackerLocal = GetActorTransform().InverseTransformPosition(Attacker->GetActorLocation());
	const FVector2D ToAttacker(AttackerLocal.X - RowOffset, AttackerLocal.Y - static_cast<double>(GetMirrorLocalY(CounterMirrorIndex)));
	const float AngleDeg = ToAttacker.IsNearlyZero() ? 0.f
		: static_cast<float>(FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(ToAttacker.GetSafeNormal().X, -1.0, 1.0))));
	if (AngleDeg > CounterHalfAngle)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Silver, TEXT("[카운터] 정면이 아님"));
		}
		return false;
	}

	bCountered = true;
	StopRow();
	Phase = EEchidnaMirrorWallPhase::Falling;

	UE_LOG(LogLoA, Warning, TEXT("[MirrorWall] 카운터 성공 — 거울 %d번, 진행 %.0f / %.0f"), CounterMirrorIndex, RowOffset, TravelDistance);

	// 카운터 성공 — 다음 줄을 바로 칠 수 있게 카운터 스킬 쿨타임 즉시 초기화
	if (ALoACharacter* AttackerCharacter = Cast<ALoACharacter>(Attacker))
	{
		if (AttackerCharacter->SkillManager)
		{
			AttackerCharacter->SkillManager->ResetCounterSkillCooldowns();
		}
	}

	if (AEchidnaBoss* BossPtr = Boss.Get())
	{
		const FVector MirrorTop = MirrorMeshes[CounterMirrorIndex]->GetComponentLocation() + FVector(0.f, 0.f, MirrorHeight * 0.5f + 40.f);
		BossPtr->SpawnCounterText(MirrorTop);
	}
	return true;
}
