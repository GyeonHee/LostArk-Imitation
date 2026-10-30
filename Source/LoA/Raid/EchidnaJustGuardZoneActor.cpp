#include "Raid/EchidnaJustGuardZoneActor.h"
#include "Raid/EchidnaBoss.h"
#include "LoACharacter.h"
#include "LoA.h"
#include "ProceduralMeshComponent.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// 원판은 반지름 100으로 한 번만 만들고 컴포넌트 스케일로 크기를 바꾼다 (매 틱 메시 재생성 안 함)
	constexpr float JustGuardUnitRadius = 100.f;
}

AEchidnaJustGuardZoneActor::AEchidnaJustGuardZoneActor()
{
	PrimaryActorTick.bCanEverTick = true;

	BackMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BackMesh"));
	SetRootComponent(BackMesh);
	BackMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BackMesh->SetCastShadow(false);

	FillMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("FillMesh"));
	FillMesh->SetupAttachment(BackMesh);
	FillMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FillMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
		TEXT("/Game/Free_Magic/Demo/LevelPrototyping/Materials/M_MirrorLaser.M_MirrorLaser"));
	if (MatFinder.Succeeded())
	{
		ZoneMaterial = MatFinder.Object;
	}
}

void AEchidnaJustGuardZoneActor::Activate(ALoACharacter* InPlayer, AEchidnaBoss* InBoss, float InRadius)
{
	Player = InPlayer;
	Boss = InBoss;
	Radius = FMath::Max(100.f, InRadius);

	BuildDisc(BackMesh, JustGuardUnitRadius, FloorOffset);
	BuildDisc(FillMesh, JustGuardUnitRadius, FloorOffset + 1.f);
	if (ZoneMaterial)
	{
		BackMID = UMaterialInstanceDynamic::Create(ZoneMaterial, this);
		BackMID->SetVectorParameterValue(ColorParameterName, BackColor);
		BackMesh->SetMaterial(0, BackMID);
		FillMID = UMaterialInstanceDynamic::Create(ZoneMaterial, this);
		FillMID->SetVectorParameterValue(ColorParameterName, FillColor);
		FillMesh->SetMaterial(0, FillMID);
	}
	// 배경은 처음부터 전체 크기 — 채움은 0에서 시작. 자식 스케일은 부모 스케일과 곱해지므로 채움은 비율만
	const float BackScale = Radius / JustGuardUnitRadius;
	BackMesh->SetWorldScale3D(FVector(BackScale, BackScale, 1.f));
	FillMesh->SetRelativeScale3D(FVector(0.01f, 0.01f, 1.f));

	Elapsed = 0.f;
	bActive = true;
	UE_LOG(LogLoA, Log, TEXT("[JustGuard] 장판 시작 — 반지름 %.0f, 게이지 %.2f초, 판정 -%.2f/+%.2f초, 방향 ±%.0f도"),
		Radius, FillDuration, EarlyWindow, LateWindow, GuardHalfAngle);
}

void AEchidnaJustGuardZoneActor::BuildDisc(UProceduralMeshComponent* Mesh, float DiscRadius, float Z)
{
	constexpr int32 Segments = 64;
	TArray<FVector> Verts;
	TArray<int32> Tris;
	Verts.Add(FVector(0.f, 0.f, Z));
	for (int32 s = 0; s <= Segments; ++s)
	{
		const float Angle = 2.f * PI * s / Segments;
		Verts.Add(FVector(FMath::Cos(Angle) * DiscRadius, FMath::Sin(Angle) * DiscRadius, Z));
	}
	for (int32 s = 1; s <= Segments; ++s)
	{
		// 양면 감김 — 다른 PMC 장판과 같은 방식
		Tris.Append({ 0, s, s + 1, 0, s + 1, s });
	}
	Mesh->CreateMeshSection(0, Verts, Tris, TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), false);
}

void AEchidnaJustGuardZoneActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bActive || bFinished) return;
	Elapsed += DeltaTime;

	if (bResolved)
	{
		if (Elapsed - ResolvedAt >= ResultLingerTime)
		{
			bFinished = true;
			SetActorHiddenInGame(true);
			SetLifeSpan(2.f);  // 진행자가 결과를 읽고 지우지만, 못 지워도 남지 않게
		}
		return;
	}

	const float Fill = FMath::Clamp(Elapsed / FMath::Max(0.01f, FillDuration), 0.01f, 1.f);
	FillMesh->SetRelativeScale3D(FVector(Fill, Fill, 1.f));
	if (FillMID)
	{
		const bool bInWindow = Elapsed >= FillDuration - EarlyWindow;
		FillMID->SetVectorParameterValue(ColorParameterName, bInWindow ? WindowColor : FillColor);
	}

	// 잘못 눌렀으면 게이지가 다 찬 순간, 안 눌렀으면 늦은 창까지 기다린 뒤 실패
	if ((bPressed && !bPressSuccess && Elapsed >= FillDuration) || (!bPressed && Elapsed >= FillDuration + LateWindow))
	{
		Resolve(false);
	}
}

bool AEchidnaJustGuardZoneActor::TryJustGuard(APawn* Pawn, const FVector& AimLocation)
{
	if (!IsAwaitingInput() || !Pawn || Pawn != Player.Get()) return false;
	bPressed = true;

	const float Delta = Elapsed - FillDuration;  // 음수 = 일찍
	const bool bTimingOk = Delta >= -EarlyWindow && Delta <= LateWindow;

	const FVector PlayerLoc = Pawn->GetActorLocation();
	const FVector ToCenter = (GetActorLocation() - PlayerLoc).GetSafeNormal2D();
	const FVector ToAim = (AimLocation - PlayerLoc).GetSafeNormal2D();
	const float AngleDeg = (ToCenter.IsNearlyZero() || ToAim.IsNearlyZero()) ? 0.f
		: FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct(ToCenter, ToAim)), -1.f, 1.f)));
	const bool bDirectionOk = FVector::Dist2D(GetActorLocation(), PlayerLoc) <= IgnoreAngleDistance || AngleDeg <= GuardHalfAngle;

	UE_LOG(LogLoA, Log, TEXT("[JustGuard] G 입력 — 타이밍 %+.0fms(%s), 방향 %.0f도(%s)"),
		Delta * 1000.f, bTimingOk ? TEXT("O") : TEXT("X"), AngleDeg, bDirectionOk ? TEXT("O") : TEXT("X"));

	if (bTimingOk && bDirectionOk)
	{
		bPressSuccess = true;
		Resolve(true);
	}
	else if (GEngine)
	{
		const FString Why = !bTimingOk ? (Delta < 0.f ? TEXT("너무 일찍") : TEXT("너무 늦게")) : TEXT("방향이 다름");
		GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Silver, FString::Printf(TEXT("[저스트 가드] %s"), *Why));
	}
	return true;
}

void AEchidnaJustGuardZoneActor::Resolve(bool bInSuccess)
{
	if (bResolved) return;
	bResolved = true;
	bSuccess = bInSuccess;
	ResolvedAt = Elapsed;

	FillMesh->SetRelativeScale3D(FVector(1.f, 1.f, 1.f));
	if (FillMID)
	{
		FillMID->SetVectorParameterValue(ColorParameterName, bSuccess ? SuccessColor : FailColor);
	}

	if (!bSuccess)
	{
		if (ALoACharacter* Char = Player.Get())
		{
			Char->ReceiveDamage(Char->GetMaxHP() * DamageMaxHPRatio, ELoADamageKind::Fixed);
		}
	}
	ShowResultLabel(bSuccess);
	UE_LOG(LogLoA, Warning, TEXT("[JustGuard] %s"), bSuccess ? TEXT("성공") : TEXT("실패 — 최대 HP 비율 피해"));
}

void AEchidnaJustGuardZoneActor::ShowResultLabel(bool bInSuccess)
{
	AEchidnaBoss* B = Boss.Get();
	const ALoACharacter* Char = Player.Get();
	if (!B || !Char) return;
	B->SpawnLabelText(Char->GetActorLocation() + FVector(0.f, 0.f, 160.f),
		bInSuccess ? SuccessText : FailText,
		bInSuccess ? B->CounterTextColor : FLinearColor(1.f, 0.25f, 0.2f, 1.f),
		B->CounterTextScale);
}
