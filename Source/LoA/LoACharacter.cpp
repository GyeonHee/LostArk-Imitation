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

	const int32 OldGauge = CharmGauge;
	CharmGauge = FMath::Clamp(CharmGauge + Amount, 0, MaxCharmGauge);

	// 스택별 개별 타이머가 아니라 전체 유지시간 하나 — 맞을 때마다(이미 최대 스택이어도) CharmGaugeStackDuration으로
	// 통째로 리셋됨. 이 시간 안에 재히트가 없으면 ClearCharmGauge()가 스택 전부를 한 번에 0으로 되돌림
	GetWorldTimerManager().SetTimer(CharmGaugeTimerHandle, this, &ALoACharacter::ClearCharmGauge, CharmGaugeStackDuration, false);

	if (CharmGauge != OldGauge)
	{
		OnCharmGaugeChanged.Broadcast(CharmGauge);
	}

	if (CharmGauge >= MaxCharmGauge && !bIsCharmed)
	{
		bIsCharmed = true;
		OnCharmedChanged.Broadcast(true);
	}
}

void ALoACharacter::ClearCharmGauge()
{
	CharmGauge = 0;
	OnCharmGaugeChanged.Broadcast(CharmGauge);

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

void ALoACharacter::ApplyPull(const FVector& TargetLocation, float PullStrength)
{
	FVector ToTarget = TargetLocation - GetActorLocation();
	ToTarget.Z = 0.f;
	if (ToTarget.IsNearlyZero()) return;

	// 수평 속도만 덮어쓰고(bXYOverride=true) 수직 속도는 그대로 둠(bZOverride=false) —
	// 넉다운과 달리 위로 띄우지 않으므로 bConstrainToPlane을 풀 필요가 없음
	LaunchCharacter(ToTarget.GetSafeNormal() * PullStrength, true, false);
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
