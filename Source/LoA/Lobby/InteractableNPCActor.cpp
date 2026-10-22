#include "Lobby/InteractableNPCActor.h"
#include "UI/NPCWorldWidgets.h"
#include "Animation/AnimationAsset.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AInteractableNPCActor::AInteractableNPCActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// 캡슐 — 플레이어가 통과하지 못하게 막고, 커서 판정(Visibility)도 받는다
	CapsuleComp = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	CapsuleComp->InitCapsuleSize(42.f, 92.f);
	CapsuleComp->SetCollisionProfileName(TEXT("Pawn"));
	CapsuleComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	RootComponent = CapsuleComp;

	MeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	MeshComp->SetupAttachment(CapsuleComp);
	// UE5 마네킹은 +Y를 보고 있어서 -90 돌려 액터 정면(+X)에 맞춘다. 발이 캡슐 바닥에 오도록 절반 높이만큼 내림
	MeshComp->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -92.f), FRotator(0.f, -90.f, 0.f));
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannequinMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
	if (MannequinMesh.Succeeded())
	{
		MeshComp->SetSkeletalMesh(MannequinMesh.Object);
	}
	static ConstructorHelpers::FObjectFinder<UAnimationAsset> Idle(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
	if (Idle.Succeeded())
	{
		IdleAnimation = Idle.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Highlight(TEXT("/Game/LostArk/UI/NPC/M_NPCHighlight.M_NPCHighlight"));
	if (Highlight.Succeeded())
	{
		HighlightMaterial = Highlight.Object;
	}

	NameComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("NameComp"));
	NameComp->SetupAttachment(CapsuleComp);
	NameComp->SetWidgetSpace(EWidgetSpace::Screen);
	NameComp->SetDrawAtDesiredSize(true);
	NameComp->SetWidgetClass(UNPCNameWidget::StaticClass());
	NameComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// [G] 대화하기 — NPC 몸 왼쪽 아래에 (피벗 X > 1이면 기준점 왼쪽으로 밀림)
	PromptComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("PromptComp"));
	PromptComp->SetupAttachment(CapsuleComp);
	PromptComp->SetWidgetSpace(EWidgetSpace::Screen);
	PromptComp->SetDrawAtDesiredSize(true);
	PromptComp->SetWidgetClass(UInteractPromptWidget::StaticClass());
	PromptComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PromptComp->SetRelativeLocation(FVector(0.f, 0.f, -20.f));
	PromptComp->SetPivot(FVector2D(1.35f, 0.5f));
	PromptComp->SetHiddenInGame(true);
}

void AInteractableNPCActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	NameComp->SetRelativeLocation(FVector(0.f, 0.f, NameHeight - CapsuleComp->GetUnscaledCapsuleHalfHeight()));
}

void AInteractableNPCActor::BeginPlay()
{
	Super::BeginPlay();

	if (IdleAnimation)
	{
		MeshComp->PlayAnimation(IdleAnimation, true);
	}

	NameComp->SetRelativeLocation(FVector(0.f, 0.f, NameHeight - CapsuleComp->GetUnscaledCapsuleHalfHeight()));
	NameComp->InitWidget();
	if (UNPCNameWidget* Label = Cast<UNPCNameWidget>(NameComp->GetUserWidgetObject()))
	{
		Label->SetNames(NPCTitle, NPCName);
	}

	PromptComp->InitWidget();
	if (UUserWidget* Prompt = PromptComp->GetUserWidgetObject())
	{
		Prompt->TakeWidget();
	}

	if (HighlightMaterial)
	{
		HighlightMID = UMaterialInstanceDynamic::Create(HighlightMaterial, this);
		HighlightMID->SetVectorParameterValue(TEXT("Color"), HighlightColor);
		HighlightMID->SetScalarParameterValue(TEXT("Intensity"), HighlightIntensity);
	}
}

void AInteractableNPCActor::SetHighlighted(bool bInHighlighted)
{
	if (bHighlighted == bInHighlighted) return;
	bHighlighted = bInHighlighted;
	// 오버레이 머티리얼 = 메시를 한 번 더 그 머티리얼로 덧그림 — 원래 머티리얼은 안 건드림(보스 카운터 발광과 같은 방식)
	MeshComp->SetOverlayMaterial(bHighlighted ? HighlightMID.Get() : nullptr);
}

void AInteractableNPCActor::SetPromptVisible(bool bVisible)
{
	if (bPromptVisible == bVisible) return;
	bPromptVisible = bVisible;
	PromptComp->SetHiddenInGame(!bVisible);
}

bool AInteractableNPCActor::IsPawnInRange(const APawn* Pawn) const
{
	if (!Pawn) return false;
	return FVector::DistSquared2D(Pawn->GetActorLocation(), GetActorLocation()) <= FMath::Square(InteractRadius);
}
