#include "Raid/EchidnaClashPlatformActor.h"
#include "UI/NPCWorldWidgets.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// 엔진 기본 Cylinder는 높이 100cm(Z축), 지름 100cm, 피벗이 중심
	constexpr float ClashPlatformCylinderSize = 100.f;
}

AEchidnaClashPlatformActor::AEchidnaClashPlatformActor()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	DiscMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DiscMesh"));
	DiscMesh->SetupAttachment(Root);
	DiscMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DiscMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderFinder.Succeeded())
	{
		DiscMesh->SetStaticMesh(CylinderFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
		TEXT("/Game/Free_Magic/Demo/LevelPrototyping/Materials/M_MirrorLaser.M_MirrorLaser"));
	if (MatFinder.Succeeded())
	{
		DiscMaterial = MatFinder.Object;
	}

	// [G] 격돌 — NPC 프롬프트와 같은 위젯, 발판 가운데 조금 위
	PromptComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("PromptComp"));
	PromptComp->SetupAttachment(Root);
	PromptComp->SetWidgetSpace(EWidgetSpace::Screen);
	PromptComp->SetDrawAtDesiredSize(true);
	PromptComp->SetWidgetClass(UInteractPromptWidget::StaticClass());
	PromptComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PromptComp->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	PromptComp->SetHiddenInGame(true);
}

void AEchidnaClashPlatformActor::BeginPlay()
{
	Super::BeginPlay();

	if (DiscMaterial)
	{
		DiscMID = UMaterialInstanceDynamic::Create(DiscMaterial, this);
		DiscMID->SetVectorParameterValue(ColorParameterName, DiscColor);
		DiscMesh->SetMaterial(0, DiscMID);
	}

	// 원판 아랫면이 액터 위치(타일 윗면)에 오도록 두께 절반만큼 올린다
	DiscMesh->SetRelativeLocation(FVector(0.f, 0.f, DiscThickness * 0.5f));
	ApplyDiscScale(RiseDuration > 0.f ? 0.f : 1.f);

	PromptComp->InitWidget();
	if (UInteractPromptWidget* Prompt = Cast<UInteractPromptWidget>(PromptComp->GetUserWidgetObject()))
	{
		Prompt->SetPrompt(FText::FromString(TEXT("G")), PromptActionText);
	}
}

void AEchidnaClashPlatformActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Elapsed += DeltaTime;

	if (RiseDuration > 0.f && Elapsed <= RiseDuration)
	{
		// ease-out — 빠르게 솟았다가 부드럽게 멈춤
		const float T = FMath::Clamp(Elapsed / RiseDuration, 0.f, 1.f);
		ApplyDiscScale(1.f - FMath::Square(1.f - T));
	}

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	bPlayerInRange = bInteractable && IsPawnInRange(PlayerPawn);
	SetPromptVisible(bPlayerInRange);

	if (DiscMID)
	{
		const float Pulse = FMath::Lerp(PulseMin, 1.f, 0.5f + 0.5f * FMath::Sin(Elapsed * PulseSpeed));
		const FLinearColor Base = bPlayerInRange ? DiscInRangeColor : DiscColor;
		DiscMID->SetVectorParameterValue(ColorParameterName,
			FLinearColor(Base.R * Pulse, Base.G * Pulse, Base.B * Pulse, Base.A));
	}
}

bool AEchidnaClashPlatformActor::IsPawnInRange(const APawn* Pawn) const
{
	if (!Pawn) return false;
	return FVector::DistSquared2D(Pawn->GetActorLocation(), GetActorLocation()) <= FMath::Square(InteractRadius);
}

bool AEchidnaClashPlatformActor::TryInteract(APawn* Pawn)
{
	if (!bInteractable || !IsPawnInRange(Pawn)) return false;
	OnInteracted.Broadcast(Pawn);
	return true;
}

void AEchidnaClashPlatformActor::SetInteractable(bool bInInteractable)
{
	bInteractable = bInInteractable;
	if (!bInteractable)
	{
		SetPromptVisible(false);
	}
}

void AEchidnaClashPlatformActor::ApplyDiscScale(float Alpha)
{
	const float XY = FMath::Max(0.01f, Alpha) * DiscRadius * 2.f / ClashPlatformCylinderSize;
	DiscMesh->SetRelativeScale3D(FVector(XY, XY, DiscThickness / ClashPlatformCylinderSize));
}

void AEchidnaClashPlatformActor::SetPromptVisible(bool bVisible)
{
	if (bPromptVisible == bVisible) return;
	bPromptVisible = bVisible;
	PromptComp->SetHiddenInGame(!bVisible);
}
