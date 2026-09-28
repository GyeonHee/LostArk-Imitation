#include "Lobby/LobbyZoneActor.h"
#include "GameFramework/Pawn.h"
#include "UI/ZoneLabelWidget.h"
#include "UI/ZoneNoticeWidget.h"
#include "ProceduralMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// 양면 감김으로 추가 — 카메라가 어느 쪽에서 보든 보이게 (HexTile 테두리와 같은 방식)
	void AddZoneQuad(TArray<FVector>& Verts, TArray<int32>& Tris,
		const FVector& A, const FVector& B, const FVector& C, const FVector& D)
	{
		const int32 Base = Verts.Num();
		Verts.Add(A); Verts.Add(B); Verts.Add(C); Verts.Add(D);
		Tris.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
		Tris.Append({ Base, Base + 2, Base + 1, Base, Base + 3, Base + 2 });
	}
}

ALobbyZoneActor::ALobbyZoneActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	ZoneMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ZoneMesh"));
	ZoneMesh->SetupAttachment(RootComponent);
	ZoneMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ZoneMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MatFinder(
		TEXT("/Game/Free_Magic/Demo/LevelPrototyping/Materials/M_MirrorLaser.M_MirrorLaser"));
	if (MatFinder.Succeeded())
	{
		ZoneMaterial = MatFinder.Object;
		ZoneMesh->SetMaterial(0, MatFinder.Object);
		ZoneMesh->SetMaterial(1, MatFinder.Object);
	}

	// Screen 스페이스라 탑다운 카메라 각도와 무관하게 항상 같은 크기로 보인다 (매혹 게이지·데미지 숫자와 같은 방식)
	LabelComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("LabelComp"));
	LabelComp->SetupAttachment(RootComponent);
	LabelComp->SetWidgetSpace(EWidgetSpace::Screen);
	LabelComp->SetDrawAtDesiredSize(true);
	LabelComp->SetWidgetClass(UZoneLabelWidget::StaticClass());
	LabelComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ALobbyZoneActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// 에디터에서 반지름·변 개수를 바꾸면 바로 모양이 보이게
	BuildZoneMesh();
	ApplyColors(1.f);
}

void ALobbyZoneActor::BeginPlay()
{
	Super::BeginPlay();

	BuildZoneMesh();
	ApplyColors(1.f);

	LabelComp->SetRelativeLocation(FVector(0.f, 0.f, LabelHeight));
	LabelComp->InitWidget();
	if (UZoneLabelWidget* Label = Cast<UZoneLabelWidget>(LabelComp->GetUserWidgetObject()))
	{
		Label->SetLabel(LabelText, LabelColor);
	}
	LabelComp->SetVisibility(!LabelText.IsEmpty());
}

void ALobbyZoneActor::BuildZoneMesh()
{
	const int32 N = FMath::Max(3, BorderSides);
	const float HalfStep = PI / N;
	// 변 두께가 어디서나 BorderWidth가 되도록 꼭짓점 쪽은 1/cos(반각)만큼 더 들어간다
	const float InnerRadius = FMath::Max(0.f, ZoneRadius - BorderWidth / FMath::Cos(HalfStep));

	auto Corner = [&](int32 Index, float Radius)
	{
		const float Angle = FMath::DegreesToRadians(BorderAngleOffset) + 2.f * HalfStep * Index;
		return FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, ZOffset);
	};

	// 섹션 0 — 테두리 (바닥 링 + 빛나는 띠)
	TArray<FVector> Verts;
	TArray<int32> Tris;
	for (int32 i = 0; i < N; ++i)
	{
		const FVector OuterA = Corner(i, ZoneRadius);
		const FVector OuterB = Corner(i + 1, ZoneRadius);
		const FVector InnerA = Corner(i, InnerRadius);
		const FVector InnerB = Corner(i + 1, InnerRadius);
		AddZoneQuad(Verts, Tris, InnerA, OuterA, OuterB, InnerB);

		if (BorderWallHeight > 0.f)
		{
			const FVector Up(0.f, 0.f, BorderWallHeight);
			AddZoneQuad(Verts, Tris, OuterA, OuterB, OuterB + Up, OuterA + Up);
		}
	}
	TArray<FVector> Normals;
	Normals.Init(FVector::UpVector, Verts.Num());
	TArray<FVector2D> UVs;
	UVs.Init(FVector2D::ZeroVector, Verts.Num());
	ZoneMesh->CreateMeshSection(0, Verts, Tris, Normals, UVs, TArray<FColor>(), TArray<FProcMeshTangent>(), false);

	// 섹션 1 — 안쪽 바닥 채움 (중심에서 팬)
	Verts.Reset();
	Tris.Reset();
	Verts.Add(FVector(0.f, 0.f, ZOffset * 0.5f));
	for (int32 i = 0; i < N; ++i)
	{
		FVector P = Corner(i, InnerRadius);
		P.Z = ZOffset * 0.5f;
		Verts.Add(P);
	}
	for (int32 i = 0; i < N; ++i)
	{
		const int32 A = 1 + i;
		const int32 B = 1 + (i + 1) % N;
		Tris.Append({ 0, A, B, 0, B, A });
	}
	Normals.Init(FVector::UpVector, Verts.Num());
	UVs.Init(FVector2D::ZeroVector, Verts.Num());
	ZoneMesh->CreateMeshSection(1, Verts, Tris, Normals, UVs, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
	ZoneMesh->SetMeshSectionVisible(1, FillColor.A > 0.f);
}

void ALobbyZoneActor::ApplyColors(float BorderIntensity)
{
	// 슬롯의 MID는 컴포넌트 머티리얼 오버라이드로 레벨에 같이 저장되고, BorderMID/FillMID는 Transient라 PIE 복사본·리인스턴싱 뒤엔 비어 있다.
	//  - 슬롯에 **이 액터 소속** MID가 있으면 재사용 (PIE 복사본은 MID도 같이 복사돼 소속이 맞다)
	//  - 아니면(옛 액터 소속·비어 있음·원본 그대로) ZoneMaterial을 부모로 새로 만든다
	// 버그였던 것: 슬롯 머티리얼을 부모로 삼았더니 ①PIE에선 MID를 부모로 MID를 만들어 "not a valid parent" ②BP 컴파일 리인스턴싱 뒤엔
	// 옛 액터 소속 MID가 버려져 슬롯이 비어 MID를 못 만들고 회색 기본 머티리얼로 그려졌다
	auto EnsureMID = [this](int32 Slot, TObjectPtr<UMaterialInstanceDynamic>& OutMID)
	{
		UMaterialInterface* SlotMat = ZoneMesh->GetMaterial(Slot);
		if (OutMID && SlotMat == OutMID) return;

		UMaterialInstanceDynamic* Existing = Cast<UMaterialInstanceDynamic>(SlotMat);
		if (Existing && Existing->GetOuter() == this)
		{
			OutMID = Existing;
			return;
		}

		UMaterialInterface* BaseMat = ZoneMaterial ? ZoneMaterial.Get() : (Existing ? Existing->Parent.Get() : SlotMat);
		if (!BaseMat) return;
		OutMID = UMaterialInstanceDynamic::Create(BaseMat, this);
		ZoneMesh->SetMaterial(Slot, OutMID);
	};
	EnsureMID(0, BorderMID);
	EnsureMID(1, FillMID);

	if (BorderMID)
	{
		const FLinearColor& Base = bUseCurrentBorderColor ? CurrentBorderColor : BorderColor;
		FLinearColor Color = Base * BorderIntensity;
		Color.A = Base.A;
		BorderMID->SetVectorParameterValue(ColorParameterName, Color);
	}
	if (FillMID)
	{
		FillMID->SetVectorParameterValue(ColorParameterName, FillColor);
	}
}

bool ALobbyZoneActor::IsInsideZone(const FVector& WorldLocation) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
	const float Dist = FVector2D(Local.X, Local.Y).Size();
	if (Dist < KINDA_SMALL_NUMBER) return true;

	// 정N각형: 점이 속한 변의 바깥 법선 방향으로 투영한 거리가 변심거리(apothem) 이하면 안쪽
	const int32 N = FMath::Max(3, BorderSides);
	const float Step = 2.f * PI / N;
	float Angle = FMath::Atan2(Local.Y, Local.X) - FMath::DegreesToRadians(BorderAngleOffset);
	Angle = FMath::Fmod(Angle + 4.f * PI, 2.f * PI);
	const float EdgeNormalAngle = FMath::FloorToFloat(Angle / Step) * Step + Step * 0.5f;
	const float Apothem = ZoneRadius * FMath::Cos(Step * 0.5f);
	return Dist * FMath::Cos(Angle - EdgeNormalAngle) <= Apothem;
}

void ALobbyZoneActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (PulseAmount > 0.f)
	{
		PulseTime += DeltaSeconds;
		ApplyColors(1.f + PulseAmount * FMath::Sin(PulseTime * PulseSpeed));
	}

	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const bool bInside = Player && IsInsideZone(Player->GetActorLocation());

	if (bInside != bPlayerInside)
	{
		bPlayerInside = bInside;
		if (bInside)
		{
			OnPlayerEnterZone(Player);
		}
		else
		{
			OnPlayerExitZone(Player);
		}
	}

	if (bInside)
	{
		TickPlayerInZone(Player, DeltaSeconds);
	}
}

void ALobbyZoneActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HideNotice();
	Super::EndPlay(EndPlayReason);
}

void ALobbyZoneActor::ShowNotice(const FText& Title, const FText& Message, const FText& Counter)
{
	if (!NoticeWidget)
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
		if (!PC || !PC->IsLocalController()) return;

		NoticeWidget = CreateWidget<UZoneNoticeWidget>(PC, UZoneNoticeWidget::StaticClass());
		if (!NoticeWidget) return;
		// 미니맵(5)보다 위, 스킬트리(10)보다 아래. 트리는 AddToViewport에서 만들어지므로 글자는 그 뒤에 넣는다
		NoticeWidget->AddToViewport(8);
	}

	NoticeWidget->SetNotice(Title, Message);
	NoticeWidget->SetCounter(Counter);
}

void ALobbyZoneActor::HideNotice()
{
	if (NoticeWidget)
	{
		NoticeWidget->RemoveFromParent();
		NoticeWidget = nullptr;
	}
}

void ALobbyZoneActor::SetCurrentBorderColor(const FLinearColor& NewColor)
{
	CurrentBorderColor = NewColor;
	bUseCurrentBorderColor = true;
	ApplyColors(1.f);
}
