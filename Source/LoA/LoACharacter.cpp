// Copyright Epic Games, Inc. All Rights Reserved.

#include "LoACharacter.h"
#include "Raid/EchidnaBoss.h"
#include "LoAPlayerController.h"
#include "Engine/DamageEvents.h"
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
#include "Item/BattleItemComponent.h"
#include "Item/InventoryComponent.h"
#include "Stat/StatComponent.h"
#include "Item/EquipmentComponent.h"
#include "Item/GemComponent.h"
#include "Stat/EngravingComponent.h"
#include "Stat/ArkPassiveComponent.h"

ALoACharacter::ALoACharacter()
{
	HP = 190000.f;
	MaxHP = 190000.f;
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

	BattleItems = CreateDefaultSubobject<UBattleItemComponent>(TEXT("BattleItems"));
	Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));
	StatComponent = CreateDefaultSubobject<UStatComponent>(TEXT("StatComponent"));
	Equipment = CreateDefaultSubobject<UEquipmentComponent>(TEXT("Equipment"));
	Gems = CreateDefaultSubobject<UGemComponent>(TEXT("Gems"));
	Engravings = CreateDefaultSubobject<UEngravingComponent>(TEXT("Engravings"));
	ArkPassive = CreateDefaultSubobject<UArkPassiveComponent>(TEXT("ArkPassive"));

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
		BaseMaxMP   = MaxMP;
		MP          = MaxMP;
		MPRegenRate = CharacterData->MPRegenRate;
		BaseMoveSpeed = CharacterData->MoveSpeed;
		GetCharacterMovement()->MaxWalkSpeed = BaseMoveSpeed;
	}

	// 공격력·최대 HP·이동속도는 스탯 계산 결과로 덮어쓴다 (DA가 없으면 컴포넌트가 아무것도 안 해서 위/생성자 값 유지)
	if (StatComponent)
	{
		StatComponent->OnStatsChanged.AddUObject(this, &ALoACharacter::HandleStatsChanged);
		StatComponent->InitializeFromData(CharacterData);
	}
}

void ALoACharacter::HandleStatsChanged(const FCombatStats& NewStats)
{
	AttackPower = static_cast<float>(NewStats.AttackPower);

	if (!FMath::IsNearlyEqual(MaxHP, NewStats.MaxHP))
	{
		MaxHP = NewStats.MaxHP;
		if (!bIsDead)
		{
			// 스폰 중(장비가 처음 들어오는 동안)엔 꽉 채우고, 그 뒤엔 로아처럼 장비를 바꿔도 현재 HP는 그대로 — 최대치만 바뀐다(줄면 잘림)
			HP = bHPSettled ? FMath::Clamp(HP, 1.f, MaxHP) : MaxHP;
		}
		OnHPChanged.Broadcast(HP);
	}

	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->MaxWalkSpeed = BaseMoveSpeed * NewStats.MoveSpeedMultiplier;
	}

	// 최대 마나 증가 각인 — 현재 마나 비율은 유지
	if (BaseMaxMP > 0.f)
	{
		const float NewMaxMP = BaseMaxMP * NewStats.MaxMPMultiplier + NewStats.MaxMPFlat;
		if (!FMath::IsNearlyEqual(NewMaxMP, MaxMP))
		{
			const float Ratio = MaxMP > 0.f ? MP / MaxMP : 1.f;
			MaxMP = NewMaxMP;
			MP = FMath::Clamp(MaxMP * Ratio, 0.f, MaxMP);
			OnMPChanged.Broadcast(MP);
		}
	}
}

void ALoACharacter::BeginPlay()
{
	Super::BeginPlay();

	// 컴포넌트 BeginPlay(장비 장착 → 스탯 재계산)가 끝났다 — 여기까지는 최대 HP로 채우고, 이후 장비 변경은 현재 HP를 건드리지 않음
	if (!bIsDead)
	{
		HP = MaxHP;
		OnHPChanged.Broadcast(HP);
	}
	bHPSettled = true;

	if (CameraBoom)
	{
		DefaultCameraArmLength = CameraBoom->TargetArmLength;
		DefaultCameraRotation = CameraBoom->GetRelativeRotation();
	}

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
		RestoreMP(MaxMP * MPRegenRate * (StatComponent ? StatComponent->GetStats().MPRegenMultiplier : 1.f));
		MPRegenAccum -= 1.f;
	}

	TickPullDrag(DeltaSeconds);

	if (CameraBoom && DefaultCameraArmLength > 0.f)
	{
		const float TargetArm = CameraArmOverride > 0.f ? CameraArmOverride : DefaultCameraArmLength;
		if (!FMath::IsNearlyEqual(CameraBoom->TargetArmLength, TargetArm, 0.5f))
		{
			CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArm, DeltaSeconds, CameraZoomInterpSpeed);
		}

		// SpringArm은 절대 회전이라 RelativeRotation이 곧 월드 회전
		const FRotator TargetRot = bCameraRotationOverride ? CameraRotationOverride : DefaultCameraRotation;
		const FRotator CurrentRot = CameraBoom->GetRelativeRotation();
		if (!CurrentRot.Equals(TargetRot, 0.05f))
		{
			CameraBoom->SetRelativeRotation(FMath::RInterpTo(CurrentRot, TargetRot, DeltaSeconds, CameraZoomInterpSpeed));
		}
	}
}

void ALoACharacter::SetCameraZoomOverride(float ArmLength)
{
	CameraArmOverride = ArmLength;
}

void ALoACharacter::ClearCameraZoomOverride()
{
	CameraArmOverride = -1.f;
}

bool ALoACharacter::TryConsumeTickDamage(FName Source, float Interval)
{
	const UWorld* World = GetWorld();
	if (!World) return true;

	const double Now = World->GetTimeSeconds();
	if (const double* Last = LastTickDamageTimes.Find(Source))
	{
		// 같은 장판이 자기 다음 틱을 프레임 오차로 놓치지 않도록 90%만 요구
		if (Now - *Last < Interval * 0.9)
		{
			return false;
		}
	}
	LastTickDamageTimes.Add(Source, Now);
	return true;
}

void ALoACharacter::SetCameraRotationOverride(FRotator Rotation)
{
	bCameraRotationOverride = true;
	CameraRotationOverride = FRotator(Rotation.Pitch, Rotation.Yaw, 0.f);
}

void ALoACharacter::ClearCameraRotationOverride()
{
	bCameraRotationOverride = false;
}

void ALoACharacter::ExecuteDash_Implementation(const FVector& TargetLocation)
{
	const FVector Dir = (TargetLocation - GetActorLocation()).GetSafeNormal2D();
	if (!SkillManager || Dir.IsNearlyZero()) return;

	const FSkillData DashData = SkillManager->GetSlotSkillData(USkillManagerComponent::DashSlotIndex);
	LaunchCharacter(Dir * DashData.DashImpulse, true, false);
	if (Engravings) Engravings->NotifyMovementSkill();  // 추진력 (아크 패시브의 "점멸"은 소서리스 X — ASorceress)
}

float ALoACharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	// 피해 종류는 DamageTypeClass로 — 아무것도 안 넘긴 패턴(UDamageType)은 마법
	ELoADamageKind Kind = ELoADamageKind::Magical;
	if (const UClass* TypeClass = DamageEvent.DamageTypeClass.Get())
	{
		if (TypeClass->IsChildOf(ULoATrueDamageType::StaticClass())) Kind = ELoADamageKind::Fixed;
		else if (TypeClass->IsChildOf(ULoAPhysicalDamageType::StaticClass())) Kind = ELoADamageKind::Physical;
	}
	ReceiveDamage(DamageAmount, Kind);
	return DamageAmount;
}

void ALoACharacter::ReceiveDamage(float DamageAmount, ELoADamageKind Kind)
{
	if (IsImmune()) return;

	// 로아 방어력 — 받는 피해 × (1 - 방어력 / (방어력 + 6500)). 패턴 수치는 "1640 시작 세트가 맞는 양"으로 적혀 있어서
	// 보스 배율(IncomingDamageScale)로 방어 전 크기로 되돌린 뒤 깎는다 → 시작 세트는 예전과 같은 양, 재련할수록 덜 아프다
	if (Kind != ELoADamageKind::Fixed && StatComponent)
	{
		const FCombatStats& S = StatComponent->GetStats();
		const float Reduction = Kind == ELoADamageKind::Physical ? S.PhysDamageReduction : S.MagicDamageReduction;
		DamageAmount *= AEchidnaBoss::GetIncomingDamageScale(this) * (1.f - Reduction) * S.IncomingDamageMultiplier;  // 원한 받는 피해 +20%
	}

	// 플레이어를 때리는 건 전부 보스 쪽(패턴·똥장판)이라 여기 한 곳에서 광폭화 배율을 건다.
	// "최대 HP × N" 기믹(Fixed)은 비율 그대로 — 거울잇기 실패 70%가 광폭화 중 즉사로 바뀌지 않게
	if (Kind != ELoADamageKind::Fixed)
	{
		DamageAmount *= AEchidnaBoss::GetEnrageDamageMultiplier(this);
	}

	// ── 각인 피격 효과 ──
	const FCombatStats* EngStats = StatComponent ? &StatComponent->GetStats() : nullptr;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (EngStats && Kind != ELoADamageKind::Fixed)
	{
		// 굳은 의지 — 밀려날 때(넉다운·끌려감)
		if (EngStats->PushedDamageReduction > 0.f && (bIsKnockedDown || bIsPulled)) DamageAmount *= 1.f - EngStats->PushedDamageReduction;
		// 불굴 — 잃은 HP에 비례
		if (EngStats->FortitudeMax > 0.f && MaxHP > 0.f) DamageAmount *= 1.f - EngStats->FortitudeMax * (1.f - HP / MaxHP);
		// 여신의 가호 — 확률로 -46% (10초에 한 번)
		if (EngStats->DivineChance > 0.f && Now >= DivineReadyTime && FMath::FRand() < EngStats->DivineChance)
		{
			DamageAmount *= 1.f - LoAStatRules::DivineReduction;
			DivineReadyTime = Now + LoAStatRules::DivineCooldown;
		}
	}
	// 위기 모면 무적 중 — 피해는 받지 않고 모아 뒀다가 끝날 때 40% 회복
	if (bCrisisActive)
	{
		CrisisAccumulated += DamageAmount;
		return;
	}
	// 긴급구조 보호막이 먼저 받는다
	if (ShieldHP > 0.f)
	{
		const float Absorbed = FMath::Min(ShieldHP, DamageAmount);
		ShieldHP -= Absorbed;
		DamageAmount -= Absorbed;
	}
	// 위기 모면 — 치명적인 피해를 받으면 3초 무적. "최대 HP × N" 즉사 기믹(Fixed)은 막지 않는다 — 기믹은 기믹으로 풀어야 해서
	if (EngStats && Kind != ELoADamageKind::Fixed && EngStats->CrisisEvasionCooldown > 0.f && HP - DamageAmount <= 0.f && Now >= CrisisReadyTime)
	{
		bCrisisActive = true;
		CrisisAccumulated = DamageAmount;
		CrisisReadyTime = Now + EngStats->CrisisEvasionCooldown;
		GetWorldTimerManager().SetTimer(CrisisTimer, this, &ALoACharacter::EndCrisisEvasion, LoAStatRules::CrisisDuration, false);
		UE_LOG(LogTemp, Log, TEXT("[Engraving] 위기 모면 발동"));
		return;
	}

	HP = FMath::Max(HP - DamageAmount, 0.f);
	OnHPChanged.Broadcast(HP);

	// 긴급구조 — HP 30% 미만이 되는 순간 최대 HP 50% 보호막 6초
	if (EngStats && EngStats->EmergencyRescueCooldown > 0.f && HP > 0.f && MaxHP > 0.f && HP / MaxHP < LoAStatRules::RescueHPThreshold && Now >= RescueReadyTime)
	{
		ShieldHP = MaxHP * LoAStatRules::RescueShieldRatio;
		RescueReadyTime = Now + EngStats->EmergencyRescueCooldown;
		GetWorldTimerManager().SetTimer(RescueTimer, this, &ALoACharacter::EndEmergencyRescue, LoAStatRules::RescueDuration, false);
		UE_LOG(LogTemp, Log, TEXT("[Engraving] 긴급구조 발동 — 보호막 %.0f"), ShieldHP);
	}

	if (HP <= 0.f)
	{
		Die();
	}
}

void ALoACharacter::EndCrisisEvasion()
{
	bCrisisActive = false;
	Heal(CrisisAccumulated * LoAStatRules::CrisisHealRatio);
	CrisisAccumulated = 0.f;
}

void ALoACharacter::EndEmergencyRescue()
{
	// 6초 뒤 남은 보호막의 50%를 HP로
	if (ShieldHP > 0.f) Heal(ShieldHP * LoAStatRules::RescueHealRatio);
	ShieldHP = 0.f;
}

void ALoACharacter::Die()
{
	if (bIsDead) return;
	bIsDead = true;

	// 하던 걸 전부 끊고 제자리에 세운다 (기절·붙잡힘과 같은 방식)
	if (SkillManager)
	{
		SkillManager->CancelActiveCastSkill();
		SkillManager->CancelPendingRangeMove();
	}
	GetCharacterMovement()->StopMovementImmediately();

	// 매혹 중이었으면 풀어서 컨트롤러의 무작위 행동 타이머도 멈추게 한다
	if (bIsCharmed)
	{
		GetWorldTimerManager().ClearTimer(CharmedTimerHandle);
		EndCharm();
	}

	UE_LOG(LogTemp, Log, TEXT("[Death] 플레이어 사망"));
	OnDeathVisualChanged(true);
	OnDied.Broadcast();
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

void ALoACharacter::RestoreFullStatus()
{
	HP = MaxHP;
	OnHPChanged.Broadcast(HP);
	MP = MaxMP;
	OnMPChanged.Broadcast(MP);
}

void ALoACharacter::Heal(float Amount)
{
	if (bIsDead || Amount <= 0.f) return;
	if (StatComponent) Amount *= StatComponent->GetStats().HealingMultiplier;  // 저주받은 인형 회복 -25%
	HP = FMath::Min(MaxHP, HP + Amount);
	OnHPChanged.Broadcast(HP);
}

void ALoACharacter::AddCharmGauge(int32 Amount)
{
	if (IsImmune()) return;

	if (Amount <= 0) return;

	// 매혹 중 재히트로 지속시간이 연장되면 "매혹은 CharmedDuration만큼만"이 깨진다. 스택도 이미 최대라 할 일이 없음
	if (bIsCharmed) return;

	const int32 OldGauge = CharmGauge;
	CharmGauge = FMath::Clamp(CharmGauge + Amount, 0, MaxCharmGauge);

	if (CharmGauge != OldGauge)
	{
		OnCharmGaugeChanged.Broadcast(CharmGauge);

		// 매혹 스택이 쌓이면 보스 정산 게이지도 같이 오른다 (3스택 도달 시 추가 보너스)
		AEchidnaBoss::NotifyCharmStackGained(this, CharmGauge >= MaxCharmGauge);
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

void ALoACharacter::ApplyKnockdown(const FVector& SourceLocation, float HopScale)
{
	if (IsImmune()) return;

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

	// 클릭 이동(RequestDirectMove)으로 걷던 중이면 CMC에 남은 요청 속도가 낙하 중 CalcVelocity에서 다시 적용돼
	// 튕겨나가는 속도를 상쇄한다(보스 쪽으로 걸어오다 맞으면 제자리에 멈추던 문제) — 요청 이동을 먼저 지운다
	GetCharacterMovement()->StopActiveMovement();
	if (ALoAPlayerController* PC = Cast<ALoAPlayerController>(GetController()))
	{
		PC->CancelAutoMove();
	}

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

	HopScale = FMath::Max(HopScale, 0.f);
	const float ArcScale = FMath::Sqrt(HopScale);
	const FVector LaunchVelocity = AwayFromSource * KnockdownHopStrength * HopScale + FVector(0.f, 0.f, KnockdownHopUpwardStrength * ArcScale);
	LaunchCharacter(LaunchVelocity, true, true);

	// 착지 여부(MovementMode)를 폴링하지 않고 고정 시간 타이머로 정착 시점을 직접 보장 —
	// bForceNextFloorCheck 등 엔진 내부 사정으로 Falling→Walking 전환이 같은 프레임에 즉시 일어나버려도
	// (특히 여러 거울이 같은 프레임에 겹쳐 때릴 때) 최소 이 시간만큼은 "튕겨나가는 중"으로 유지된다.
	// 맞을 때마다 이 타이머를 새로 시작하므로, 연속 타격 동안은 계속 갱신되어 눕지 않는다
	GetWorldTimerManager().SetTimer(KnockdownSettleTimerHandle, this, &ALoACharacter::SettleKnockdown, KnockdownHopSettleTime * FMath::Max(ArcScale, 1.f), false);

	if (bFirstHit)
	{
		OnKnockdownChanged.Broadcast(true);
		OnKnockdownVisualChanged(true);
	}
}

void ALoACharacter::ApplyPull(const FVector& TargetLocation, float PullSpeed)
{
	if (IsImmune()) return;

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
	if (IsImmune()) return;

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

void ALoACharacter::SetHeldByPattern(bool bHeld)
{
	if (bIsHeld == bHeld) return;
	bIsHeld = bHeld;

	if (bIsHeld)
	{
		// 기절과 같은 방식으로 하던 걸 끊고 제자리에 세운다
		if (SkillManager)
		{
			SkillManager->CancelActiveCastSkill();
			SkillManager->CancelPendingRangeMove();
		}
		GetCharacterMovement()->StopMovementImmediately();
	}

	OnHeldVisualChanged(bIsHeld);
}

void ALoACharacter::ApplyStun(float Duration)
{
	if (IsImmune()) return;

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
