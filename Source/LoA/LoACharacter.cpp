// Copyright Epic Games, Inc. All Rights Reserved.

#include "LoACharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "Camera/CameraComponent.h"
#include "Components/DecalComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/Material.h"
#include "Engine/World.h"
#include "Components/WidgetComponent.h"
#include "UI/CharmGaugeWidget.h"

ALoACharacter::ALoACharacter()
{
	HP = 100000.f;
	MaxHP = 100000.f;
	MP = 5000.f;
	MaxMP = 5000.f;

	// Set size for player capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate character to camera direction
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 640.f, 0.f);
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->bSnapToPlaneAtStart = true;
	GetCharacterMovement()->MaxAcceleration = 99999.0f;

	// Create the skill manager component
	SkillManager = CreateDefaultSubobject<USkillManagerComponent>(TEXT("SkillManager"));

	// Create the camera boom component
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));

	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->TargetArmLength = 800.f;
	CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
	CameraBoom->bDoCollisionTest = false;

	// Create the camera component
	TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));

	TopDownCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCameraComponent->bUsePawnControlRotation = false;

	// 매혹 게이지 머리 위 UI — Screen 스페이스라 탑다운 카메라 각도와 무관하게 항상 화면쪽으로 투영되어 보임.
	// WidgetClass는 여기서 하드코딩하지 않고 캐릭터 BP에서 WBP_CharmGauge로 직접 할당(다른 BP 전용 값들과 동일 컨벤션)
	CharmGaugeWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("CharmGaugeWidgetComponent"));
	CharmGaugeWidgetComponent->SetupAttachment(RootComponent);
	CharmGaugeWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, 160.f));
	CharmGaugeWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	CharmGaugeWidgetComponent->SetDrawSize(FVector2D(150.f, 60.f));
	CharmGaugeWidgetComponent->SetDrawAtDesiredSize(true);

	// Activate ticking in order to update the cursor every frame.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void ALoACharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (CharacterData)
	{
		MaxHP       = CharacterData->MaxHP;
		HP          = MaxHP;
		MaxMP       = CharacterData->MaxMP;
		MP          = MaxMP;
		MPRegenRate = CharacterData->MPRegenRate;
		AttackPower = CharacterData->AttackPower;
		GetCharacterMovement()->MaxWalkSpeed = CharacterData->MoveSpeed;
	}
}

void ALoACharacter::BeginPlay()
{
	Super::BeginPlay();

	GetCharacterMovement()->MaxAcceleration = 99999.0f;

	OnCharmGaugeChanged.AddUObject(this, &ALoACharacter::HandleCharmGaugeChanged);
	HandleCharmGaugeChanged(CharmGauge);
}

void ALoACharacter::HandleCharmGaugeChanged(int32 NewGauge)
{
	if (!CharmGaugeWidgetComponent) return;

	if (UCharmGaugeWidget* Widget = Cast<UCharmGaugeWidget>(CharmGaugeWidgetComponent->GetUserWidgetObject()))
	{
		Widget->SetStacks(NewGauge);
	}
}

void ALoACharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

	MPRegenAccum += DeltaSeconds;
	if (MPRegenAccum >= 1.f)
	{
		RestoreMP(MaxMP * MPRegenRate);
		MPRegenAccum -= 1.f;
	}

	TickPullDrag(DeltaSeconds);
}

void ALoACharacter::ExecuteDash_Implementation(const FVector& TargetLocation)
{
	const FVector Dir = (TargetLocation - GetActorLocation()).GetSafeNormal2D();
	if (!SkillManager || Dir.IsNearlyZero()) return;

	const FSkillData DashData = SkillManager->GetSlotSkillData(USkillManagerComponent::DashSlotIndex);
	LaunchCharacter(Dir * DashData.DashImpulse, true, false);
}

float ALoACharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	ReceiveDamage(DamageAmount);
	return DamageAmount;
}

void ALoACharacter::ReceiveDamage(float DamageAmount)
{
	HP = FMath::Max(HP - DamageAmount, 0.f);
	OnHPChanged.Broadcast(HP);
}

bool ALoACharacter::ConsumeMP(float Amount)
{
	if (MP < Amount) return false;
	MP = FMath::Max(MP - Amount, 0.f);
	OnMPChanged.Broadcast(MP);
	return true;
}

void ALoACharacter::RestoreMP(float Amount)
{
	MP = FMath::Min(MP + Amount, MaxMP);
	OnMPChanged.Broadcast(MP);
}

void ALoACharacter::AddCharmGauge(int32 Amount)
{
	if (Amount <= 0) return;

	// 매혹 중 재히트로 지속시간이 연장되면 "매혹은 CharmedDuration만큼만"이 깨진다. 스택도 이미 최대라 할 일이 없음
	if (bIsCharmed) return;

	const int32 OldGauge = CharmGauge;
	CharmGauge = FMath::Clamp(CharmGauge + Amount, 0, MaxCharmGauge);

	if (CharmGauge != OldGauge)
	{
		OnCharmGaugeChanged.Broadcast(CharmGauge);
	}

	if (CharmGauge >= MaxCharmGauge)
	{
		// 매혹 진입 — 스택 감소는 더 이상 의미가 없고(어차피 끝나면 통째로 0) CharmedDuration 뒤 EndCharm()이 정리한다
		GetWorldTimerManager().ClearTimer(CharmGaugeTimerHandle);
		GetWorldTimerManager().SetTimer(CharmedTimerHandle, this, &ALoACharacter::EndCharm, CharmedDuration, false);

		bIsCharmed = true;
		OnCharmedChanged.Broadcast(true);
		return;
	}

	// 루핑 타이머 — 재히트 없이 CharmGaugeStackDuration이 지날 때마다 1스택씩 깎인다 (2→1→0)
	GetWorldTimerManager().SetTimer(CharmGaugeTimerHandle, this, &ALoACharacter::DecayCharmGauge, CharmGaugeStackDuration, true);
}

void ALoACharacter::DecayCharmGauge()
{
	if (CharmGauge > 0)
	{
		--CharmGauge;
		OnCharmGaugeChanged.Broadcast(CharmGauge);
	}

	// 남은 스택이 있으면 루핑 타이머가 그대로 다음 주기를 센다
	if (CharmGauge <= 0)
	{
		GetWorldTimerManager().ClearTimer(CharmGaugeTimerHandle);
	}
}

void ALoACharacter::EndCharm()
{
	GetWorldTimerManager().ClearTimer(CharmGaugeTimerHandle);

	if (CharmGauge != 0)
	{
		CharmGauge = 0;
		OnCharmGaugeChanged.Broadcast(CharmGauge);
	}

	if (bIsCharmed)
	{
		bIsCharmed = false;
		OnCharmedChanged.Broadcast(false);
	}
}

void ALoACharacter::ApplyKnockdown(const FVector& SourceLocation)
{
	// 이미 완전히 누운 상태면 재입력 무시. 정착 타이머 대기 중(bKnockdownAirborne)엔 재히트를 허용해서
	// 타이머를 계속 갱신 — 거울 레이저처럼 틱마다 맞는 패턴은 그동안 계속 공중에 떠 있는 것처럼 보임
	if (bIsKnockedDown && !bKnockdownAirborne) return;

	const bool bFirstHit = !bIsKnockedDown;

	bIsKnockedDown = true;
	bKnockdownAirborne = true;

	if (SkillManager)
	{
		SkillManager->CancelActiveCastSkill();
		SkillManager->CancelPendingRangeMove();
	}

	GetWorldTimerManager().ClearTimer(KnockdownTimerHandle);

	// 평상시엔 top-down 클릭이동 때문에 Z를 평면에 고정해두는데(생성자의 bConstrainToPlane=true),
	// 넉다운 중엔 잠깐 풀어줘서 LaunchCharacter의 수직 임펄스가 실제로 뒤로 튕겨나가는 아크로 보이게 함
	GetCharacterMovement()->bConstrainToPlane = false;

	FVector AwayFromSource = GetActorLocation() - SourceLocation;
	AwayFromSource.Z = 0.f;
	if (AwayFromSource.IsNearlyZero())
	{
		AwayFromSource = -GetActorForwardVector();
	}
	AwayFromSource = AwayFromSource.GetSafeNormal();

	const FVector LaunchVelocity = AwayFromSource * KnockdownHopStrength + FVector(0.f, 0.f, KnockdownHopUpwardStrength);
	LaunchCharacter(LaunchVelocity, true, true);

	// 착지 여부(MovementMode)를 폴링하지 않고 고정 시간 타이머로 정착 시점을 직접 보장 —
	// bForceNextFloorCheck 등 엔진 내부 사정으로 Falling→Walking 전환이 같은 프레임에 즉시 일어나버려도
	// (특히 여러 거울이 같은 프레임에 겹쳐 때릴 때) 최소 이 시간만큼은 "튕겨나가는 중"으로 유지된다.
	// 맞을 때마다 이 타이머를 새로 시작하므로, 연속 타격 동안은 계속 갱신되어 눕지 않는다
	GetWorldTimerManager().SetTimer(KnockdownSettleTimerHandle, this, &ALoACharacter::SettleKnockdown, KnockdownHopSettleTime, false);

	if (bFirstHit)
	{
		OnKnockdownChanged.Broadcast(true);
		OnKnockdownVisualChanged(true);
	}
}

void ALoACharacter::ApplyPull(const FVector& TargetLocation, float PullSpeed)
{
	// 넉다운 중이면 이미 더 강한 행동불능 상태이므로 무시
	if (bIsKnockedDown) return;

	FVector ToTarget = TargetLocation - GetActorLocation();
	ToTarget.Z = 0.f;
	if (ToTarget.IsNearlyZero()) return;

	// 보스와 완전히 겹치지 않도록 목표 지점 앞 PullStopDistance에서 멈추게 한다.
	// 이미 그보다 가까우면 제자리를 목적지로 삼아 "멈추기만" 하고 끝낸다
	const float Distance = ToTarget.Size();
	const FVector Direction = ToTarget / Distance;
	const float TravelDistance = FMath::Max(Distance - PullStopDistance, 0.f);

	PullDestination = GetActorLocation() + Direction * TravelDistance;
	PullSpeedCmS = FMath::Max(PullSpeed, 1.f);
	PullDragElapsed = 0.f;
	bPullDragging = false;

	const bool bFirstHit = !bIsPulled;
	bIsPulled = true;

	// 경직/기절과 동일 — 캐스팅과 사거리 이동 대기를 끊고 제자리에 세운다.
	// 여기서는 아직 끌지 않는다. PullHoldDuration 동안 "발이 묶인" 상태로 멈춰 있다가 드래그가 시작됨
	if (SkillManager)
	{
		SkillManager->CancelActiveCastSkill();
		SkillManager->CancelPendingRangeMove();
	}
	GetCharacterMovement()->StopMovementImmediately();

	GetWorldTimerManager().SetTimer(PullHoldTimerHandle, this, &ALoACharacter::BeginPullDrag, PullHoldDuration, false);

	// 끌려간 뒤에도 패턴이 끝날 때까지 계속 묶여 있으므로, 패턴이 비정상 종료돼 ReleasePull()이
	// 안 불려도 영구 속박이 되지 않도록 안전 타이머를 건다
	GetWorldTimerManager().SetTimer(PullSafetyTimerHandle, this, &ALoACharacter::ReleasePull, PullMaxHoldTime, false);

	if (bFirstHit)
	{
		OnPullVisualChanged(true);
	}
}

void ALoACharacter::BeginPullDrag()
{
	if (!bIsPulled) return;

	bPullDragging = true;
	PullDragElapsed = 0.f;
}

void ALoACharacter::TickPullDrag(float DeltaSeconds)
{
	if (!bPullDragging) return;

	PullDragElapsed += DeltaSeconds;

	FVector ToDestination = PullDestination - GetActorLocation();
	ToDestination.Z = 0.f;
	const float Remaining = ToDestination.Size();

	// 물리 임펄스가 아니라 위치를 직접 옮긴다 — 임펄스는 마찰/지형에 따라 도달 거리가 들쭉날쭉해서
	// "확실히 보스 앞까지 끌려온다"를 보장하지 못했음. bSweep=true라 벽은 여전히 막아준다
	const float Step = PullSpeedCmS * DeltaSeconds;
	if (Remaining <= Step || PullDragElapsed >= PullMaxDragTime)
	{
		if (Remaining > KINDA_SMALL_NUMBER)
		{
			AddActorWorldOffset(ToDestination, true);
		}
		FinishPullDrag();
		return;
	}

	AddActorWorldOffset(ToDestination / Remaining * Step, true);
}

void ALoACharacter::FinishPullDrag()
{
	// 도착해도 bIsPulled는 유지 — 끌려온 자리에서 패턴이 끝날 때까지 묶여 있어야 하므로
	// 해제는 ReleasePull()(끌기를 건 패턴의 ExitState)이나 PullMaxHoldTime 안전 타이머가 담당한다
	bPullDragging = false;
	GetCharacterMovement()->StopMovementImmediately();
}

void ALoACharacter::ReleasePull()
{
	if (!bIsPulled) return;

	GetWorldTimerManager().ClearTimer(PullHoldTimerHandle);
	GetWorldTimerManager().ClearTimer(PullSafetyTimerHandle);
	bIsPulled = false;
	bPullDragging = false;
	OnPullVisualChanged(false);
}

void ALoACharacter::SettleKnockdown()
{
	if (!bIsKnockedDown || !bKnockdownAirborne) return;

	bKnockdownAirborne = false;

	// DisableMovement()가 중력/낙하를 즉시 멈춰버리므로, 타이머가 끝난 시점에 아직 공중에 떠 있으면
	// (연속 타격으로 여러 번 재발사되어 정착 시간(KnockdownHopSettleTime)보다 낙하가 오래 걸리는 경우)
	// 그대로 허공에 얼어붙는다. 그래서 실제 착지를 기다리는 대신 바닥까지 트레이스해 직접 스냅시킨다.
	const FVector CurrentLocation = GetActorLocation();
	const FVector TraceStart = CurrentLocation + FVector(0.f, 0.f, 200.f);
	const FVector TraceEnd = CurrentLocation - FVector(0.f, 0.f, 2000.f);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		FVector LandedLocation = CurrentLocation;
		LandedLocation.Z = Hit.ImpactPoint.Z + HalfHeight;
		SetActorLocation(LandedLocation, false);
	}

	GetCharacterMovement()->bConstrainToPlane = true;	// ApplyKnockdown에서 풀어준 평면 구속을 다시 걸어 평소 top-down 이동으로 복귀
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	GetWorldTimerManager().SetTimer(KnockdownTimerHandle, this, &ALoACharacter::GetUpFromKnockdown, KnockdownDuration, false);

	OnKnockdownSettled();
}

void ALoACharacter::GetUpFromKnockdown()
{
	if (!bIsKnockedDown) return;

	bIsKnockedDown = false;
	bKnockdownAirborne = false;
	GetWorldTimerManager().ClearTimer(KnockdownTimerHandle);
	GetWorldTimerManager().ClearTimer(KnockdownSettleTimerHandle);
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	OnKnockdownChanged.Broadcast(false);
	OnKnockdownVisualChanged(false);
}

void ALoACharacter::ApplyStagger()
{
	// 넉다운 중이면 이미 더 강한 행동불능 상태이므로 무시
	if (bIsKnockedDown) return;

	const bool bFirstHit = !bIsStaggered;
	bIsStaggered = true;

	// 넉다운과 달리 캐릭터를 띄우거나 던지지 않음 — 캐스팅/사거리 이동 대기 중이던 스킬만 취소하고 제자리에 멈춤
	if (SkillManager)
	{
		SkillManager->CancelActiveCastSkill();
		SkillManager->CancelPendingRangeMove();
	}

	GetCharacterMovement()->StopMovementImmediately();

	// 경직 중 재히트하면 타이머가 갱신되어 계속 경직 유지 (넉다운의 정착 타이머 갱신과 동일한 패턴)
	GetWorldTimerManager().SetTimer(StaggerTimerHandle, this, &ALoACharacter::EndStagger, StaggerDuration, false);

	if (bFirstHit)
	{
		OnStaggerVisualChanged(true);
	}
}

void ALoACharacter::EndStagger()
{
	if (!bIsStaggered) return;

	bIsStaggered = false;
	OnStaggerVisualChanged(false);
}

void ALoACharacter::ApplyStun(float Duration)
{
	// 넉다운 중이면 이미 더 강한 행동불능 상태이므로 무시
	if (bIsKnockedDown) return;

	const bool bFirstHit = !bIsStunned;
	bIsStunned = true;

	// 경직과 동일 — 캐릭터를 띄우거나 던지지 않고 캐스팅/사거리 이동 대기 중이던 스킬만 취소하고 제자리에 멈춤
	if (SkillManager)
	{
		SkillManager->CancelActiveCastSkill();
		SkillManager->CancelPendingRangeMove();
	}

	GetCharacterMovement()->StopMovementImmediately();

	// 기절 중 재히트하면 타이머가 갱신되어 계속 기절 유지
	GetWorldTimerManager().SetTimer(StunTimerHandle, this, &ALoACharacter::EndStun, Duration, false);

	if (bFirstHit)
	{
		OnStunVisualChanged(true);
	}
}

void ALoACharacter::EndStun()
{
	if (!bIsStunned) return;

	bIsStunned = false;
	OnStunVisualChanged(false);
}

bool ALoACharacter::TryInstantGetUp()
{
	// 공중에 떠 있는 동안은(아직 착지 전) 즉시 기상 불가 — 완전히 누운 뒤에만 사용 가능
	if (!bIsKnockedDown || bKnockdownAirborne) return false;
	if (!SkillManager || SkillManager->IsSlotOnCooldown(USkillManagerComponent::GetUpSlotIndex)) return false;

	SkillManager->TriggerCooldown(USkillManagerComponent::GetUpSlotIndex);
	GetUpFromKnockdown();
	return true;
}
