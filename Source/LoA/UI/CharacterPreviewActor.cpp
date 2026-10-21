#include "UI/CharacterPreviewActor.h"
#include "Animation/AnimationAsset.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	TAutoConsoleVariable<float> CVarPreviewExposure(
		TEXT("LoA.Preview.Exposure"), 0.f,
		TEXT("캐릭터 정보 창 미리보기 노출 보정(EV). 창을 다시 열면 반영"));

	TAutoConsoleVariable<float> CVarPreviewLight(
		TEXT("LoA.Preview.Light"), 1.f,
		TEXT("캐릭터 정보 창 미리보기 라이트 세기 배율. 창을 다시 열면 반영"));

	// 창의 미리보기 칸(344x516)의 약 2배 — TAA 대신 줄여 보여주는 슈퍼샘플링으로 가장자리를 다듬는다
	constexpr int32 Preview_TargetWidth = 704;
	constexpr int32 Preview_TargetHeight = 1056;

	// 캐릭터 정면(+X) 쪽에서 허리 높이로 — 수평 FOV 28도, 2:3 렌더 타깃이면 캐릭터가 세로의 약 2/3를 채운다
	const FVector Preview_CameraOffset(380.f, 0.f, 90.f);

	void Preview_SetupLight(UPointLightComponent* Light, const FVector& Location, const FLinearColor& Color)
	{
		Light->SetRelativeLocation(Location);
		Light->SetLightColor(Color);
		Light->SetAttenuationRadius(1200.f);
		// 그림자·MegaLights 끔 — 확률적 샘플링/가상 섀도맵은 작은 캡처에서 노이즈가 남기 쉽다.
		// 혼자 서 있는 미리보기라 자기 그림자는 거의 티가 안 난다
		Light->SetCastShadows(false);
		Light->bAllowMegaLights = false;
		// 반사광 없음 — 노멀맵 위 작은 하이라이트가 하얀 점처럼 반짝였음(0.35로 줄여도 남음).
		// 형태는 확산광만으로 충분히 보인다
		Light->SetSpecularScale(0.f);
		// 채널 1만 — 미리보기 메시만 비추고 레벨의 다른 것은 안 비춘다
		Light->SetLightingChannels(false, true, false);
	}
}

ACharacterPreviewActor::ACharacterPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	MeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PreviewMesh"));
	MeshComp->SetupAttachment(Root);
	// 마네킹 메시는 +Y가 정면 → -90도 돌리면 +X(카메라 쪽)를 본다. 발이 액터 원점
	MeshComp->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp->SetLightingChannels(false, true, false);
	MeshComp->bReceivesDecals = false;
	MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	static ConstructorHelpers::FObjectFinder<UAnimationAsset> Idle(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
	if (Idle.Succeeded())
	{
		IdleAnimation = Idle.Object;
	}

	// 레벨 조명(태양·스카이라이트)은 전혀 안 받으므로 이 셋이 전부 — 어느 맵에서 열어도 밝기가 같다
	// 키: 왼쪽 위 앞에서 입체감
	KeyLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(Root);
	Preview_SetupLight(KeyLight, FVector(260.f, 180.f, 240.f), FLinearColor(1.f, 0.96f, 0.92f));

	// 필: 카메라 자리에서 정면으로 — 그늘진 쪽도 어둡게 묻히지 않게
	FillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(Root);
	Preview_SetupLight(FillLight, Preview_CameraOffset + FVector(0.f, 0.f, 20.f), FLinearColor(0.92f, 0.95f, 1.f));

	// 림: 뒤에서 윤곽을 살려 검은 배경과 분리
	RimLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("RimLight"));
	RimLight->SetupAttachment(Root);
	Preview_SetupLight(RimLight, FVector(-220.f, 0.f, 240.f), FLinearColor(0.85f, 0.88f, 1.f));

	CaptureComp = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	CaptureComp->SetupAttachment(Root);
	CaptureComp->SetRelativeLocation(Preview_CameraOffset);
	CaptureComp->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	CaptureComp->FOVAngle = 28.f;
	CaptureComp->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	// 톤 커브까지 적용한 결과를 선형 16비트로 받는다. FinalColorLDR(8비트)은 톤매퍼가 양자화 디더링을 넣어서
	// 검은 배경·어두운 곳이 매 프레임 0~13으로 자글거렸음 — 캐릭터가 윤슬처럼 반짝인 마지막 원인(PIE 프레임 비교로 0이 됨 확인).
	// 선형 값이라 UI가 sRGB로 올바르게 변환해 보여준다. (SceneColorHDR은 노출 전 값이라 완전히 검게 나와서 못 씀)
	CaptureComp->CaptureSource = ESceneCaptureSource::SCS_FinalToneCurveHDR;
	CaptureComp->bCaptureEveryFrame = false;
	CaptureComp->bCaptureOnMovement = false;
	CaptureComp->bAlwaysPersistRenderingState = true;

	// 미리보기에 필요 없는 것들 — 대기·안개·모션 블러·렌즈 효과
	CaptureComp->ShowFlags.SetAtmosphere(false);
	CaptureComp->ShowFlags.SetFog(false);
	CaptureComp->ShowFlags.SetVolumetricFog(false);
	CaptureComp->ShowFlags.SetCloud(false);
	CaptureComp->ShowFlags.SetMotionBlur(false);
	CaptureComp->ShowFlags.SetLensFlares(false);
	// 레벨 스카이라이트 끔 — 라이팅 채널을 무시하는 조명이라 맵마다 밝기가 달라지던 원인
	CaptureComp->ShowFlags.SetSkyLighting(false);
	// 화면 공간 노이즈 효과 끔. TAA는 켜 둔다 — 끄면 Idle 숨쉬기에 맞춰 슈트의 가는 선·윤곽이 지글거렸음.
	// (처음 반짝임의 원인이던 반사광·8비트 디더링은 따로 없앴으므로 TAA를 켜도 반짝이지 않는다)
	CaptureComp->ShowFlags.SetAmbientOcclusion(false);
	CaptureComp->ShowFlags.SetScreenSpaceReflections(false);
	CaptureComp->ShowFlags.SetContactShadows(false);
	CaptureComp->ShowFlags.SetDistanceFieldAO(false);
	CaptureComp->ShowFlags.SetGrain(false);

	FPostProcessSettings& PP = CaptureComp->PostProcessSettings;
	PP.bOverride_AutoExposureMethod = true;
	PP.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	PP.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	PP.AutoExposureApplyPhysicalCameraExposure = false;
	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = 0.f;
	// Lumen 없이 직접광만 — 맵과 떨어진 허공이라 GI가 쓸모없고 무겁다
	PP.bOverride_DynamicGlobalIlluminationMethod = true;
	PP.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::None;
	PP.bOverride_ReflectionMethod = true;
	PP.ReflectionMethod = EReflectionMethod::None;
	PP.bOverride_bMegaLights = true;
	PP.bMegaLights = false;
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = 0.f;
}

void ACharacterPreviewActor::BeginPlay()
{
	Super::BeginPlay();

	RenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("CharacterPreviewTarget"));
	RenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA16f; // FinalToneCurveHDR은 선형 HDR이라 16비트 float
	RenderTarget->ClearColor = FLinearColor::Black;
	RenderTarget->InitAutoFormat(Preview_TargetWidth, Preview_TargetHeight);
	RenderTarget->UpdateResourceImmediate(true);
	CaptureComp->TextureTarget = RenderTarget;

	CaptureComp->ShowOnlyComponents.Reset();
	// 메시만 찍는다 — 나머지는 비어서 검은 배경이 된다
	CaptureComp->ShowOnlyComponents.Add(MeshComp);

	SetPreviewActive(false);
}

void ACharacterPreviewActor::SetupFromCharacter(const ACharacter* Source)
{
	const USkeletalMeshComponent* SourceMesh = Source ? Source->GetMesh() : nullptr;
	USkeletalMesh* Mesh = SourceMesh ? SourceMesh->GetSkeletalMeshAsset() : nullptr;
	if (!Mesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CharacterPreview] 원본 캐릭터 메시 없음 — 미리보기 비어 있음"));
		return;
	}

	MeshComp->SetSkeletalMesh(Mesh);
	MeshComp->SetRelativeScale3D(SourceMesh->GetRelativeScale3D());
	// 캐릭터 BP에서 바꾼 머티리얼까지 그대로
	for (int32 i = 0; i < SourceMesh->GetNumMaterials(); ++i)
	{
		MeshComp->SetMaterial(i, SourceMesh->GetMaterial(i));
	}

	// 캐릭터의 AnimBP는 폰(속도 등)을 읽어서 쓰지 않는다 — 단일 Idle 애니메이션만
	if (IdleAnimation && IdleAnimation->GetSkeleton() == Mesh->GetSkeleton())
	{
		MeshComp->PlayAnimation(IdleAnimation, true);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[CharacterPreview] Idle 애니메이션의 스켈레톤이 '%s'와 달라 기본 포즈로 표시"), *GetNameSafe(Mesh));
	}
}

void ACharacterPreviewActor::SetPreviewActive(bool bActive)
{
	if (bActive)
	{
		ApplyTuning();
	}
	CaptureComp->bCaptureEveryFrame = bActive;
	MeshComp->SetComponentTickEnabled(bActive);
	if (bActive)
	{
		CaptureComp->CaptureSceneDeferred();
	}
}

void ACharacterPreviewActor::AddPreviewYaw(float DeltaYaw)
{
	MeshComp->AddRelativeRotation(FRotator(0.f, DeltaYaw, 0.f));
}

void ACharacterPreviewActor::ApplyTuning()
{
	const float LightScale = FMath::Max(0.f, CVarPreviewLight.GetValueOnGameThread());
	KeyLight->SetIntensity(KeyIntensity * LightScale);
	FillLight->SetIntensity(FillIntensity * LightScale);
	RimLight->SetIntensity(RimIntensity * LightScale);
	CaptureComp->PostProcessSettings.AutoExposureBias = CVarPreviewExposure.GetValueOnGameThread();
}
