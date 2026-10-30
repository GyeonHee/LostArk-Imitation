#include "Raid/EchidnaShuffleMirrorActor.h"
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

	// 엔진 Cylinder(지름·높이 100, 중심 피벗)를 Pitch 90으로 세우면 원기둥 축이 받침 +X(정면)를 향해 얇은 타원판이 된다.
	// Pitch 90 회전에서 로컬 X → 위, 로컬 Z → 정면이므로 스케일은 (높이, 폭, 두께) 순서
	MirrorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MirrorMesh"));
	MirrorMesh->SetupAttachment(Pivot);
	MirrorMesh->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
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

	// 받침이 아니라 루트에 — 가라앉는 연출과 무관하게 제자리
	CountdownComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("CountdownComp"));
	CountdownComp->SetupAttachment(Root);
	CountdownComp->SetWidgetSpace(EWidgetSpace::Screen);
	CountdownComp->SetDrawAtDesiredSize(true);
	CountdownComp->SetWidgetClass(UWorldCountdownWidget::StaticClass());
	CountdownComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CountdownComp->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderFinder.Succeeded())
	{
		MirrorMesh->SetStaticMesh(CylinderFinder.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
		TEXT("/Game/Free_Magic/Demo/LevelPrototyping/Materials/M_MirrorLaser.M_MirrorLaser"));
	if (MatFinder.Succeeded())
	{
		BaseMaterial = MatFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PortraitMatFinder(
		TEXT("/Game/LostArk/Raid/Echidna/Pattern/M_EchidnaMirrorPortrait.M_EchidnaMirrorPortrait"));
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
	MirrorMesh->SetRelativeLocation(FVector(0.f, 0.f, H * 0.5f));
	MirrorMesh->SetRelativeScale3D(FVector(H / 100.f, W / 100.f, MirrorThickness / 100.f));
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
		MirrorMesh->SetMaterial(0, MirrorMID);
	}
	EnsurePortraitMID();
	if (bStaticPortrait) BuildPortraitSection(0.f);
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

void AEchidnaShuffleMirrorActor::BuildPortraitSection(float OffsetY)
{
	PortraitMesh->ClearAllMeshSections();
	if (!PortraitMID) return;

	const float H = MirrorHeight * CurrentScale;
	const float W = MirrorWidth * CurrentScale;
	// 앞면 = +두께/2, 1cm 띄워 Z-파이팅 방지
	const float FrontX = MirrorThickness * 0.5f + 1.f;
	const float CenterZ = H * 0.5f;
	const float RadiusY = W * 0.5f * PortraitInset;
	const float RadiusZ = H * 0.5f * PortraitInset;

	// 거울 테두리 타원(볼록 다각형)을 초상화 한 장이 차지하는 세로 띠 [OffsetY - RadiusZ, OffsetY + RadiusZ]로 자른다.
	// 초상화는 정사각 텍스처를 세로 반지름 기준으로 펴므로 폭 = 2·RadiusZ. 정지(OffsetY 0)면 타원이 띠 안이라 그대로
	TArray<FVector2D> Poly;
	constexpr int32 Segments = 40;
	for (int32 s = 0; s < Segments; ++s)
	{
		const float Angle = 2.f * PI * s / Segments;
		Poly.Add(FVector2D(FMath::Cos(Angle) * RadiusY, FMath::Sin(Angle) * RadiusZ));
	}
	auto ClipY = [](const TArray<FVector2D>& In, float Bound, bool bKeepAbove)
	{
		TArray<FVector2D> Out;
		auto Inside = [Bound, bKeepAbove](const FVector2D& P) { return bKeepAbove ? P.X >= Bound : P.X <= Bound; };
		for (int32 i = 0; i < In.Num(); ++i)
		{
			const FVector2D& A = In[i];
			const FVector2D& B = In[(i + 1) % In.Num()];
			const bool bA = Inside(A), bB = Inside(B);
			if (bA) Out.Add(A);
			if (bA != bB)
			{
				const double T = (Bound - A.X) / (B.X - A.X);
				Out.Add(FMath::Lerp(A, B, T));
			}
		}
		return Out;
	};
	Poly = ClipY(Poly, OffsetY - RadiusZ, true);
	Poly = ClipY(Poly, OffsetY + RadiusZ, false);
	if (Poly.Num() < 3) return;

	// 정면(+X)에서 보는 사람에겐 +Y가 왼쪽이므로 u는 초상화 왼쪽 끝(+Y)에서 0
	TArray<FVector> Verts;
	TArray<FVector2D> UVs;
	TArray<int32> Tris;
	for (const FVector2D& P : Poly)
	{
		Verts.Add(FVector(FrontX, P.X, CenterZ + P.Y));
		UVs.Add(FVector2D(0.5f - (P.X - OffsetY) / (2.f * RadiusZ), 0.5f - P.Y / (2.f * RadiusZ)));
	}
	for (int32 i = 1; i + 1 < Poly.Num(); ++i)
	{
		// 양쪽 감김 — PMC 앞면 방향과 무관하게 항상 보이게
		Tris.Append({ 0, i, i + 1, 0, i + 1, i });
	}
	PortraitMesh->CreateMeshSection(0, Verts, Tris, TArray<FVector>(), UVs, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
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
	BuildPortraitSection(10000.f);  // 시작 전엔 비움
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
	if (PortraitMID)
	{
		PortraitMID->SetVectorParameterValue(TEXT("Tint"), bRed && !bCounterGlow ? RedPortraitTint : NormalPortraitTint);
	}
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
		const float Travel = (MirrorWidth * 0.5f + MirrorHeight * 0.5f) * PortraitInset * CurrentScale;
		const float FromY = bPassLeftToRight ? Travel : -Travel;
		BuildPortraitSection(FMath::Lerp(FromY, -FromY, T));
		if (T >= 1.f)
		{
			bPassing = false;
			PortraitMesh->ClearAllMeshSections();
		}
	}

	if (AnimMode == EAnim::None) return;

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
