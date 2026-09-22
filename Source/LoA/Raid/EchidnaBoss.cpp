#include "EchidnaBoss.h"
#include "BossDirectionIndicatorComponent.h"
#include "UI/DamageNumberActor.h"
#include "GameFramework/CharacterMovementComponent.h"

AEchidnaBoss::AEchidnaBoss()
{
	PrimaryActorTick.bCanEverTick = false;

	// UCharacterMovementComponent 기본값(true)을 끔 — 켜져 있으면 LaunchCharacter로 물리 이동시킬 때마다
	// (예: 뒤로 빠지며 좌우장판의 후방 홉) 보스가 자기 이동 방향(예: 후방)을 향해 자동으로 회전해버려서,
	// 패턴 코드가 SetActorRotation으로 잡아둔 조준 방향과 무관하게 잠깐 홱 돌았다가 다시 스냅되는 것처럼
	// 보임 — 이 프로젝트의 모든 회전은 패턴 코드가 직접 SetActorRotation으로 관리하므로 자동 회전 불필요
	GetCharacterMovement()->bOrientRotationToMovement = false;

	// 앞/뒤 방향 표시 — 캡슐(루트)에 붙이면 보스 회전을 그대로 따라간다
	DirectionIndicator = CreateDefaultSubobject<UBossDirectionIndicatorComponent>(TEXT("DirectionIndicator"));
	DirectionIndicator->SetupAttachment(RootComponent);

	// 285줄(다인 하드) 기준 210줄이던 트리거를 솔로 210줄 기준으로 환산한 값.
	// TotalLines와 같은 값을 넣으면 풀피에서 곧바로 발동해버리므로 반드시 그보다 작아야 한다
	BigPatternThresholds.Add(FBossPatternThreshold{ 155, TEXT("MirrorCounter") });
}

void AEchidnaBoss::BeginPlay()
{
	Super::BeginPlay();

	HP = MaxHP;
	LastBroadcastLine = GetCurrentLine();

	// 컨트롤러가 보스보다 먼저 BeginPlay를 돌면 HP가 아직 0이라 빈 바를 보게 되므로 여기서 한 번 밀어준다
	OnHPChanged.Broadcast(HP, MaxHP);
}

int32 AEchidnaBoss::GetCurrentLine() const
{
	if (MaxHP <= 0.f || TotalLines <= 0)
	{
		return 0;
	}

	const float LineValue = MaxHP / static_cast<float>(TotalLines);
	return FMath::Clamp(FMath::CeilToInt(HP / LineValue), 0, TotalLines);
}

float AEchidnaBoss::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// **Super의 반환값을 쓰면 안 된다.** AActor::InternalTakeRadialDamage는 ComponentHits에서 가장 가까운
	// 충돌 지점을 찾아 거리로 배율을 구하는데, 오버랩은 잡혔어도 그 지점 계산이 실패하면 거리가
	// UE_MAX_FLT로 남아 배율이 0이 되고, ApplyRadialDamage는 MinimumDamage=0이라 결과가 통째로 0이 된다
	// (bDoFullDamage=true는 감쇠 곡선만 없앨 뿐 이 경로는 막지 못함).
	// 실제로 "가끔 데미지 0이 뜨는" 증상의 원인이었고, HP도 안 깎이고 있었다.
	// 이 프로젝트는 감쇠를 쓰지 않으므로 들어온 원본 값을 그대로 적용한다 — ALoACharacter::TakeDamage와 동일
	ReceiveDamage(DamageAmount);
	return DamageAmount;
}

void AEchidnaBoss::ReceiveDamage(float DamageAmount)
{
	HP = FMath::Clamp(HP - DamageAmount, 0.f, MaxHP);
	OnHPChanged.Broadcast(HP, MaxHP);

	SpawnDamageNumber(DamageAmount);

	const int32 NewLine = GetCurrentLine();
	if (NewLine != LastBroadcastLine)
	{
		LastBroadcastLine = NewLine;
		OnLineChanged.Broadcast(NewLine);
	}
}

void AEchidnaBoss::SpawnDamageNumber(float DamageAmount)
{
	// 0짜리 숫자는 화면 노이즈일 뿐이라 아예 띄우지 않는다 (틱 데미지 반올림으로 0이 되는 경우 등)
	if (!DamageNumberClass || DamageAmount < 0.5f) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// GetActorLocation()이 캡슐 중심이라 그대로가 곧 몸통 한가운데. 거기서 조금 올리고 흩뿌린다
	FVector SpawnLocation = GetActorLocation() + FVector(0.f, 0.f, DamageNumberHeight);
	SpawnLocation += FVector(
		FMath::FRandRange(-DamageNumberJitter, DamageNumberJitter),
		FMath::FRandRange(-DamageNumberJitter, DamageNumberJitter),
		FMath::FRandRange(-DamageNumberJitter * 0.5f, DamageNumberJitter * 0.5f));

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (ADamageNumberActor* Number = World->SpawnActor<ADamageNumberActor>(
		DamageNumberClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams))
	{
		// 카운터가 계속 커지므로 나중에 맞은 숫자일수록 앞에 그려진다
		Number->Activate(DamageAmount, ++DamageNumberCounter);
	}
}

bool AEchidnaBoss::IsPatternTriggered(FName PatternName) const
{
	return TriggeredPatterns.Contains(PatternName);
}

void AEchidnaBoss::MarkPatternTriggered(FName PatternName)
{
	TriggeredPatterns.Add(PatternName);
}
