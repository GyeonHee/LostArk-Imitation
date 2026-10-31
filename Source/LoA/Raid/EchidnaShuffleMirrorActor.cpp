#include "Raid/EchidnaShuffleMirrorActor.h"
#include "Raid/EchidnaMirrorModel.h"
#include "ProceduralMeshComponent.h"
#include "UI/NPCWorldWidgets.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AEchidnaShuffleMirrorActor::AEchidnaShuffleMirrorActor()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetupAttachment(Root);

	// SM_EchidnaMirror — 받침 원점(바닥)에 서서 +X(정면)를 본다. 크기·유리 색은 Setup에서 (EchidnaMirrorModel)
	MirrorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MirrorMesh"));
	MirrorMesh->SetupAttachment(Pivot);
	// 돌풍 토네이도의 오브젝트 타입 오버랩(AllDynamicObjects)에만 잡히면 된다 — 채널 응답은 전부 무시해서
	// 플레이어를 막거나 마우스 클릭 트레이스(Visibility)를 가로채지 않게 (거울 카운터 줄 거울과 같은 설정)
	MirrorMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MirrorMesh->SetCollisionObjectType(ECC_WorldDynamic);
	MirrorMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	MirrorMesh->SetGenerateOverlapEvents(false);

	PortraitMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("PortraitMesh"));
	PortraitMesh->SetupAttachment(Pivot);
	PortraitMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PortraitMesh->SetCastShadow(false);
	// 초상화 머티리얼이 반투명이라 같은 반투명인 유리보다 나중에 그려야 가려지지 않는다
	PortraitMesh->SetTranslucentSortPriority(1);

	// 받침이 아니라 루트에 — 가라앉는 연출과 무관하게 제자리
	CountdownComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("CountdownComp"));
	CountdownComp->SetupAttachment(Root);
	CountdownComp->SetWidgetSpace(EWidgetSpace::Screen);
	CountdownComp->SetDrawAtDesiredSize(true);
	CountdownComp->SetWidgetClass(UWorldCountdownWidget::StaticClass());
	CountdownComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CountdownComp->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> MirrorModelFinder(EchidnaMirrorModel::MeshPath);
	if (MirrorModelFinder.Succeeded())
	{
		MirrorMesh->SetStaticMesh(MirrorModelFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
		TEXT("/Game/Free_Magic/Demo/LevelPrototyping/Materials/M_MirrorLaser.M_MirrorLaser"));
	if (MatFinder.Succeeded())
	{
		BaseMaterial = MatFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PortraitMatFinder(
		TEXT("/Game/LostArk/Raid/Echidna/Pattern/M_EchidnaMirrorPortraitPass.M_EchidnaMirrorPortraitPass"));
	if (PortraitMatFinder.Succeeded())
	{
		PortraitMaterial = PortraitMatFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UTexture2D> PortraitFinder(
		TEXT("/Game/LostArk/Raid/EchidnaModling/other/Mirror/materials/textures/fx/fx_l_mirror_sden_01_cl.fx_l_mirror_sden_01_cl"));
	if (PortraitFinder.Succeeded())
	{
		PortraitTexture = PortraitFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UTexture2D> PortraitLeftFinder(
		TEXT("/Game/LostArk/Raid/EchidnaModling/other/Mirror/materials/textures/fx/fx_l_mirror_sden_02_cl.fx_l_mirror_sden_02_cl"));
	if (PortraitLeftFinder.Succeeded())
	{
		PortraitTextureFacingLeft = PortraitLeftFinder.Object;
	}
}

void AEchidnaShuffleMirrorActor::Setup(float Scale, const FVector& FaceDirection, bool bInCounterable, bool bStaticPortrait)
{
	CurrentScale = FMath::Max(0.1f, Scale);
	bCounterable = bInCounterable;

	const float H = MirrorHeight * CurrentScale;
	const float W = MirrorWidth * CurrentScale;
	EchidnaMirrorModel::Place(MirrorMesh, W, H);
	if (!bCounterable)
	{
		MirrorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 받침 +X(거울 정면)를 카메라 쪽으로 — 위로 드는 각도(Pitch)는 FaceCameraTilt만큼만
	FRotator FaceRot = FaceDirection.GetSafeNormal().Rotation();
	FaceRot.Pitch *= FaceCameraTilt;
	FaceRot.Roll = 0.f;
	Pivot->SetWorldRotation(FaceRot);

	if (BaseMaterial)
	{
		MirrorMID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		EchidnaMirrorModel::SetGlassMaterial(MirrorMesh, MirrorMID);
	}
	EnsurePortraitMID();
	BuildPortraitQuad();
	PortraitMesh->SetVisibility(bStaticPortrait);
	ApplyColors();

	CountdownComp->SetRelativeLocation(FVector(0.f, 0.f, H + CountdownHeightOffset));
	CountdownComp->InitWidget();

	// 바닥 아래에서 솟아오른다
	AnimMode = EAnim::Appear;
	AnimElapsed = 0.f;
	AnimDuration = FMath::Max(0.01f, AppearDuration);
	Pivot->SetRelativeLocation(FVector(0.f, 0.f, -GetSinkDepth()));
}

void AEchidnaShuffleMirrorActor::EnsurePortraitMID()
{
	if (PortraitMID || !PortraitMaterial || !PortraitTexture) return;
	PortraitMID = UMaterialInstanceDynamic::Create(PortraitMaterial, this);
	PortraitMID->SetTextureParameterValue(TEXT("Portrait"), PortraitTexture);
	PortraitMesh->SetMaterial(0, PortraitMID);
}

void AEchidnaShuffleMirrorActor::GetPortraitEllipse(float& OutCenterZ, float& OutRadiusY, float& OutRadiusZ) const
{
	// 유리 타원 안쪽
	const float ModelScale = EchidnaMirrorModel::FitScale(MirrorWidth * CurrentScale, MirrorHeight * CurrentScale);
	OutCenterZ = EchidnaMirrorModel::GlassCenterZ * ModelScale;
	OutRadiusY = EchidnaMirrorModel::GlassWidth * 0.5f * ModelScale * PortraitInset;
	OutRadiusZ = EchidnaMirrorModel::GlassHeight * 0.5f * ModelScale * PortraitInset;
}

void AEchidnaShuffleMirrorActor::BuildPortraitQuad()
{
	PortraitMesh->ClearAllMeshSections();
	PortraitMesh->SetRelativeLocation(FVector::ZeroVector);
	if (!PortraitMID) return;

	float CenterZ, RadiusY, RadiusZ;
	GetPortraitEllipse(CenterZ, RadiusY, RadiusZ);

	// 정사각 초상화 한 장(한 변 = 세로 지름)을 한 번만 만든다. 지나가기는 컴포넌트를 옮기고, 머티리얼
	// (M_EchidnaMirrorPortraitPass — 반투명, 불투명도 = 텍스처 알파(인물만) × 유리 타원 ClipCenter/ClipAxisY/ClipAxisZ)이 잘라낸다.
	// 예전처럼 매 틱 메시를 다시 만들어 UV를 밀면 모션 벡터가 0이라 TSR 잔상으로 그림이 뭉개지고 깨졌다
	// 유리면(x≈0)에서 1cm 띄워 Z-파이팅 방지. 정면(+X)에서 보는 사람에겐 +Y가 왼쪽이므로 u는 +Y에서 0
	const float FrontX = 1.f;
	const float Half = RadiusZ;
	const TArray<FVector> Verts = {
		FVector(FrontX, Half, CenterZ + Half), FVector(FrontX, -Half, CenterZ + Half),
		FVector(FrontX, -Half, CenterZ - Half), FVector(FrontX, Half, CenterZ - Half) };
	const TArray<FVector2D> UVs = { FVector2D(0.f, 0.f), FVector2D(1.f, 0.f), FVector2D(1.f, 1.f), FVector2D(0.f, 1.f) };
	// 양쪽 감김 — PMC 앞면 방향과 무관하게 항상 보이게
	const TArray<int32> Tris = { 0, 1, 2, 0, 2, 3, 0, 2, 1, 0, 3, 2 };
	PortraitMesh->CreateMeshSection(0, Verts, Tris, TArray<FVector>(), UVs, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
	UpdatePortraitClip();
}

void AEchidnaShuffleMirrorActor::UpdatePortraitClip()
{
	if (!PortraitMID) return;

	float CenterZ, RadiusY, RadiusZ;
	GetPortraitEllipse(CenterZ, RadiusY, RadiusZ);

	// 받침(가라앉기·기울기 포함) 기준 유리 타원을 월드로 — 머티리얼이 (u/Ry)² + (v/Rz)² > 1인 픽셀을 버린다
	const FTransform& PivotXform = Pivot->GetComponentTransform();
	const FVector Center = PivotXform.TransformPosition(FVector(1.f, 0.f, CenterZ));
	const FVector AxisY = PivotXform.GetUnitAxis(EAxis::Y) / FMath::Max(1.f, RadiusY);
	const FVector AxisZ = PivotXform.GetUnitAxis(EAxis::Z) / FMath::Max(1.f, RadiusZ);
	PortraitMID->SetVectorParameterValue(TEXT("ClipCenter"), FLinearColor(Center));
	PortraitMID->SetVectorParameterValue(TEXT("ClipAxisY"), FLinearColor(AxisY));
	PortraitMID->SetVectorParameterValue(TEXT("ClipAxisZ"), FLinearColor(AxisZ));
}

void AEchidnaShuffleMirrorActor::PlayPortraitPass(bool bLeftToRight, float Duration)
{
	EnsurePortraitMID();
	if (!PortraitMID) return;
	bPassing = true;
	bPassLeftToRight = bLeftToRight;
	PassElapsed = 0.f;
	PassDuration = FMath::Max(0.05f, Duration);
	// 인물이 치우친 쪽에서 들어온다 — 01(오른쪽 치우침)은 오른쪽→왼쪽, 02(왼쪽 치우침)는 왼쪽→오른쪽
	UTexture2D* Texture = (bLeftToRight && PortraitTextureFacingLeft) ? PortraitTextureFacingLeft.Get() : PortraitTexture.Get();
	PortraitMID->SetTextureParameterValue(TEXT("Portrait"), Texture);
	if (PortraitMesh->GetNumSections() == 0) BuildPortraitQuad();
	// 첫 위치는 타원 밖이라 마스크에 다 잘려 안 보인다
	PortraitMesh->SetRelativeLocation(FVector(0.f, GetPortraitPassTravel() * (bLeftToRight ? 1.f : -1.f), 0.f));
	PortraitMesh->SetVisibility(true);
}

void AEchidnaShuffleMirrorActor::SetRedGlow(bool bInRed)
{
	bRed = bInRed;
	ApplyColors();
}

void AEchidnaShuffleMirrorActor::SetCounterGlow(bool bGlow)
{
	bCounterGlow = bGlow;
	ApplyColors();
}

void AEchidnaShuffleMirrorActor::SetCounterReady(bool bReady)
{
	bCounterReady = bReady;
	ApplyColors();
}

void AEchidnaShuffleMirrorActor::SetCountdown(int32 Seconds)
{
	if (Seconds < 0)
	{
		CountdownComp->SetHiddenInGame(true);
		return;
	}
	if (UWorldCountdownWidget* Widget = Cast<UWorldCountdownWidget>(CountdownComp->GetUserWidgetObject()))
	{
		Widget->SetSeconds(Seconds);
	}
	CountdownComp->SetHiddenInGame(false);
}

void AEchidnaShuffleMirrorActor::ApplyColors()
{
	if (MirrorMID)
	{
		const FLinearColor& Color = bCounterGlow ? CounterColor : bCounterReady ? CounterReadyColor : bRed ? RedColor : NormalColor;
		MirrorMID->SetVectorParameterValue(ColorParameterName, Color);
	}

	// 빨간 거울은 프레임까지 발광 — 오버레이를 붙였다 뗀다(메시 머티리얼은 그대로)
	const bool bFrameRed = bRed && !bCounterGlow && !bCounterReady;
	if (bFrameRed && !FrameGlowMID && BaseMaterial)
	{
		FrameGlowMID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	}
	if (bFrameRed && FrameGlowMID)
	{
		FrameGlowMID->SetVectorParameterValue(ColorParameterName, RedFrameGlowColor);
		MirrorMesh->SetOverlayMaterial(FrameGlowMID);
	}
	else
	{
		MirrorMesh->SetOverlayMaterial(nullptr);
	}
	if (PortraitMID)
	{
		PortraitMID->SetVectorParameterValue(TEXT("Tint"), bRed && !bCounterGlow ? RedPortraitTint : NormalPortraitTint);
	}
}

float AEchidnaShuffleMirrorActor::GetPortraitPassTravel() const
{
	// 초상화(반폭 RadiusZ)가 타원(반폭 RadiusY) 밖 한쪽에서 들어와 반대쪽 밖으로 완전히 빠져나가는 거리
	float CenterZ, RadiusY, RadiusZ;
	GetPortraitEllipse(CenterZ, RadiusY, RadiusZ);
	return RadiusY + RadiusZ;
}

float AEchidnaShuffleMirrorActor::GetSinkDepth() const
{
	return MirrorHeight * CurrentScale + SinkMargin;
}

void AEchidnaShuffleMirrorActor::PlayDip(float Duration)
{
	if (AnimMode == EAnim::Vanish) return;
	AnimMode = EAnim::Dip;
	AnimElapsed = 0.f;
	AnimDuration = FMath::Max(0.01f, Duration);
}

void AEchidnaShuffleMirrorActor::Vanish(float Duration)
{
	bAcceptCounter = false;
	AnimMode = EAnim::Vanish;
	AnimElapsed = 0.f;
	AnimDuration = FMath::Max(0.01f, Duration);
}

void AEchidnaShuffleMirrorActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bPassing)
	{
		// 초상화 한 장이 거울 밖(한쪽)에서 들어와 반대쪽 밖으로 완전히 빠져나간다 — 등속이라 방향을 읽기 쉽게.
		// 화면 오른쪽 = 거울 로컬 -Y
		PassElapsed += DeltaTime;
		const float T = FMath::Clamp(PassElapsed / PassDuration, 0.f, 1.f);
		const float FromY = GetPortraitPassTravel() * (bPassLeftToRight ? 1.f : -1.f);
		PortraitMesh->SetRelativeLocation(FVector(0.f, FMath::Lerp(FromY, -FromY, T), 0.f));
		if (T >= 1.f)
		{
			bPassing = false;
			PortraitMesh->SetVisibility(false);
			PortraitMesh->SetRelativeLocation(FVector::ZeroVector);
		}
	}

	if (AnimMode == EAnim::None)
	{
		if (PortraitMesh->IsVisible()) UpdatePortraitClip();
		return;
	}

	AnimElapsed += DeltaTime;
	const float T = FMath::Clamp(AnimElapsed / AnimDuration, 0.f, 1.f);

	// Sink = 0(제자리) ~ 1(바닥 아래로 완전히 숨음)
	float Sink = 0.f;
	switch (AnimMode)
	{
	case EAnim::Appear:
		Sink = 1.f - FMath::SmoothStep(0.f, 1.f, T);
		break;
	case EAnim::Dip:
		// 40% 내려감 → 20% 숨어 있음 → 40% 올라옴
		Sink = T < 0.4f ? FMath::SmoothStep(0.f, 0.4f, T)
			: T < 0.6f ? 1.f
			: 1.f - FMath::SmoothStep(0.6f, 1.f, T);
		break;
	case EAnim::Vanish:
		Sink = FMath::SmoothStep(0.f, 1.f, T);
		break;
	default:
		break;
	}
	Pivot->SetRelativeLocation(FVector(0.f, 0.f, -GetSinkDepth() * Sink));
	if (PortraitMesh->IsVisible()) UpdatePortraitClip();

	if (T >= 1.f)
	{
		const EAnim Finished = AnimMode;
		AnimMode = EAnim::None;
		if (Finished == EAnim::Vanish)
		{
			Destroy();
		}
	}
}

bool AEchidnaShuffleMirrorActor::TryCounterHit(AActor* Attacker, UPrimitiveComponent* HitComponent)
{
	if (!bCounterable || !bAcceptCounter || AnimMode != EAnim::None || !IsValid(Attacker)) return false;
	OnCounterHit.Broadcast(this, Attacker);
	return true;
}
