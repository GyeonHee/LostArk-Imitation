#include "DamageNumberActor.h"
#include "DamageNumberWidget.h"
#include "UI/DamageNumberPool.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarDamageNumberPool(
		TEXT("LoA.DamageNumber.Pool"), 1,
		TEXT("데미지 숫자 재사용 — 1 풀 사용(기본) / 0 타격마다 스폰·파괴(성능 비교용)"));
}

ADamageNumberActor* ADamageNumberActor::Show(UWorld* World, TSubclassOf<ADamageNumberActor> Class, const FVector& Location, AActor* Owner)
{
	if (!World || !Class) return nullptr;

	if (CVarDamageNumberPool.GetValueOnGameThread() != 0)
	{
		if (UDamageNumberPool* Pool = World->GetSubsystem<UDamageNumberPool>())
		{
			return Pool->Acquire(Class, Location, Owner);
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Owner;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<ADamageNumberActor>(Class, Location, FRotator::ZeroRotator, SpawnParams);
}

void ADamageNumberActor::PrewarmPool(UWorld* World, TSubclassOf<ADamageNumberActor> Class, int32 Count, AActor* Owner)
{
	if (!World || !Class || CVarDamageNumberPool.GetValueOnGameThread() == 0) return;
	if (UDamageNumberPool* Pool = World->GetSubsystem<UDamageNumberPool>())
	{
		Pool->Prewarm(Class, Count, Owner);
	}
}

ADamageNumberActor::ADamageNumberActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// 매혹 게이지 위젯과 동일한 설정 — Screen 스페이스라 탑다운 카메라 각도와 무관하게 항상 화면 투영으로 보인다
	DamageWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("DamageWidgetComponent"));
	DamageWidgetComponent->SetupAttachment(Root);
	DamageWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	DamageWidgetComponent->SetDrawAtDesiredSize(true);
	DamageWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DamageWidgetComponent->SetGenerateOverlapEvents(false);
}

void ADamageNumberActor::BeginPlay()
{
	Super::BeginPlay();

	// 수명이 다하면 확실히 정리 — Tick에서도 Destroy하지만 Tick이 멈춘 경우를 대비한 이중 안전장치
	// 풀 소속은 파괴하지 않고 Tick에서 풀로 돌아간다
	if (!bPooled)
	{
		SetLifeSpan(Lifetime);
	}
}

void ADamageNumberActor::BeginUse(const FVector& Location, AActor* NewOwner)
{
	Elapsed = 0.f;
	SetOwner(NewOwner);
	SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
	if (DamageWidgetComponent)
	{
		DamageWidgetComponent->SetComponentTickEnabled(true);
		if (UUserWidget* Widget = DamageWidgetComponent->GetUserWidgetObject())
		{
			Widget->SetRenderOpacity(1.f);
			Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
}

void ADamageNumberActor::EndUse()
{
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
	if (DamageWidgetComponent)
	{
		// Screen 스페이스 위젯은 화면 레이어에 그대로 남아 있으므로 위젯 자체를 접어야 안 그려진다
		if (UUserWidget* Widget = DamageWidgetComponent->GetUserWidgetObject())
		{
			Widget->SetVisibility(ESlateVisibility::Collapsed);
		}
		DamageWidgetComponent->SetComponentTickEnabled(false);
	}
}

void ADamageNumberActor::Activate(float Damage, int32 SortPriority)
{
	if (!DamageWidgetComponent) return;

	// 나중에 맞은 숫자일수록 SortPriority가 커서 앞에 그려진다
	DamageWidgetComponent->SetTranslucentSortPriority(SortPriority);

	if (UDamageNumberWidget* Widget = Cast<UDamageNumberWidget>(DamageWidgetComponent->GetUserWidgetObject()))
	{
		Widget->SetDamage(Damage);
	}
}

void ADamageNumberActor::ActivateLabel(const FText& Label, FLinearColor Color, float FontSizeScale, int32 SortPriority)
{
	if (!DamageWidgetComponent) return;

	DamageWidgetComponent->SetTranslucentSortPriority(SortPriority);

	if (UDamageNumberWidget* Widget = Cast<UDamageNumberWidget>(DamageWidgetComponent->GetUserWidgetObject()))
	{
		Widget->SetLabel(Label, Color, FontSizeScale);
	}
}

void ADamageNumberActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	Elapsed += DeltaTime;

	AddActorWorldOffset(FVector(0.f, 0.f, FloatUpSpeed * DeltaTime));

	if (Lifetime <= 0.f)
	{
		return;
	}

	// FadeStartRatio까지는 완전 불투명하게 두고(읽을 시간을 준다) 그 뒤 수명 끝까지 선형으로 사라짐.
	// WidgetComponent가 아니라 안에 든 UserWidget의 RenderOpacity를 건드려야 Screen 스페이스에서도 먹는다
	const float LifeAlpha = FMath::Clamp(Elapsed / Lifetime, 0.f, 1.f);
	if (LifeAlpha > FadeStartRatio)
	{
		const float FadeAlpha = (LifeAlpha - FadeStartRatio) / FMath::Max(1.f - FadeStartRatio, KINDA_SMALL_NUMBER);
		if (UUserWidget* Widget = DamageWidgetComponent ? DamageWidgetComponent->GetUserWidgetObject() : nullptr)
		{
			Widget->SetRenderOpacity(FMath::Clamp(1.f - FadeAlpha, 0.f, 1.f));
		}
	}

	if (Elapsed >= Lifetime)
	{
		if (bPooled)
		{
			if (UDamageNumberPool* Pool = GetWorld() ? GetWorld()->GetSubsystem<UDamageNumberPool>() : nullptr)
			{
				Pool->Release(this);
				return;
			}
		}
		Destroy();
	}
}
