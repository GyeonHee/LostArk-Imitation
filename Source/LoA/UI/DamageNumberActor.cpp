#include "DamageNumberActor.h"
#include "DamageNumberWidget.h"
#include "Components/WidgetComponent.h"

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
	SetLifeSpan(Lifetime);
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
		Destroy();
	}
}
