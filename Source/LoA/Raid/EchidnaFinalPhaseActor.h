#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EchidnaFinalPhaseActor.generated.h"

class AEchidnaBoss;
class AEchidnaClashPlatformActor;
class AEchidnaShuffleMirrorActor;
class AEchidnaJustGuardZoneActor;
class AEchidnaClashQTEActor;
class AHexArena;
class ALoACharacter;
class UTexture2D;

UENUM(BlueprintType)
enum class EEchidnaFinalPhaseStep : uint8
{
	None,
	Cutscene,		// 진입 연출 — 플레이어 무적 + 붙잡힘
	MirrorShuffle,	// 거울 섞기 — 빨간 거울 공개 → 스왑 → 카운터 (오답·시간 초과면 저스트 가드), 카운터 N번 성공까지 반복
	Platform,		// 보스가 마지막 정답 거울에서 나오고 바로 앞에 격돌 발판 → 플레이어가 올라가 G
	Clash,			// QTE 격돌
	Finished
};

/**
 * 0줄 발악 페이즈 진행자 — AEchidnaBoss::BeginFinalPhase가 스폰한다.
 * StateTree가 멈춘 뒤(보스 AI 정지) 이 액터 혼자 연출 → 거울 섞기 → 격돌 발판 → QTE 격돌을 순서대로 돌리고,
 * 결과를 Boss->CompleteFinalPhase(성공 여부)로 돌려준다. 성공 = 레이드 클리어, 실패 = 레이드 실패(정비소 복귀).
 *
 * 거울 섞기: 보스가 사라지고 맵 가운데에 큰 거울 1 + 외곽 원형 8개. 외곽 하나가 RevealDuration 동안 빨갛게 빛났다 꺼지고,
 * 가운데 거울을 초상화가 PortraitPassCount번 가로지른다(왼→오 = +1, 오→왼 = -1) — 합계만큼 빨간 거울이 시계 방향(+)으로 옮겨 간다.
 * 그다음 FirstSwapDelay 뒤부터 거울 두 개씩 가라앉았다 올라오며 "내용"(빨간 거울 여부)이 바뀐다(실제 위치는 그대로) × SwapCount.
 * 그 뒤 빨간 거울에 카운터 = 성공(누적), 다른 거울 카운터·시간 초과 = 그 거울에서 저스트 가드 장판 → 새 라운드.
 * 성공이 RequiredCounterSuccesses에 닿으면 보스가 마지막 정답 거울 자리에서 나온다.
 *
 * 진입 연출은 아직 시간만 흐르는 자리표시. 보스가 광폭화 중이어도 이 액터는 실시간(광폭화 타이머는 페이즈 진입 때 멈춤)
 */
UCLASS(Blueprintable)
class LOA_API AEchidnaFinalPhaseActor : public AActor
{
	GENERATED_BODY()

public:
	AEchidnaFinalPhaseActor();

	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void StartPhase(AEchidnaBoss* InBoss);

	UFUNCTION(BlueprintPure, Category = "FinalPhase")
	EEchidnaFinalPhaseStep GetStep() const { return Step; }

	/** 테스트 명령 LoAClashResult — 진행 중인 단계를 건너뛰고 격돌 결과를 바로 낸다 */
	void ForceClashResult(bool bSuccess);

	// ── 진입 연출 (미정 — 지금은 대사 + 카메라 줌인만) ──
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Cutscene")
	float CutsceneDuration = 4.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Cutscene")
	float CutsceneCameraArmLength = 1100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Cutscene")
	FText DialogueSpeaker = NSLOCTEXT("FinalPhase", "Speaker", "에키드나");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Cutscene")
	FText DialogueLine = NSLOCTEXT("FinalPhase", "Line", "이대로 끝날 것 같으냐!");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Cutscene")
	TSoftObjectPtr<UTexture2D> DialoguePortrait;

	// ── 거울 섞기 ──
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Shuffle")
	TSubclassOf<AEchidnaShuffleMirrorActor> MirrorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Shuffle")
	TSubclassOf<AEchidnaJustGuardZoneActor> JustGuardZoneClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Shuffle", meta = (ClampMin = "2"))
	int32 OuterMirrorCount = 8;

	// 맵 가운데 타일에서 외곽 거울까지 (cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Shuffle")
	float OuterRingRadius = 950.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Shuffle")
	float CenterMirrorScale = 1.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Shuffle")
	float OuterMirrorScale = 1.2f;

	// 거울 섞기 동안 카메라 — 비스듬한 3/4 시점(로아 거울 무력화 화면). Yaw는 평소 카메라 그대로
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Shuffle")
	float ShuffleCameraArmLength = 2000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Shuffle")
	float ShuffleCameraPitch = -40.f;

	// 거울이 솟은 뒤(또는 라운드 시작 뒤) 빨간 거울이 켜지기까지 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Shuffle")
	float RevealDelay = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Shuffle")
	float RevealDuration = 1.5f;

	// 빨간 거울이 꺼진 뒤 가운데 거울 초상화가 처음 지나가기까지 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Shuffle")
	float PortraitPassDelay = 1.f;

	// 초상화가 지나가는 횟수 — 방향은 매번 랜덤, 합계(+는 시계 방향)만큼 빨간 거울이 옮겨 간다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Shuffle", meta = (ClampMin = "0"))
	int32 PortraitPassCount = 5;

	// 한 번 지나가는 시간 / 다음 번까지 빈 시간 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Shuffle")
	float PortraitPassDuration = 0.7f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Shuffle")
	float PortraitPassGap = 0.3f;

	// 마지막 초상화가 지나간 뒤 첫 스왑까지 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Shuffle")
	float FirstSwapDelay = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Shuffle", meta = (ClampMin = "1"))
	int32 SwapCount = 3;

	// 한 번 스왑에 두 거울이 가라앉았다 올라오는 시간 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Shuffle")
	float SwapDipDuration = 1.f;

	// 스왑이 끝나고 다음 스왑까지 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Shuffle")
	float SwapGap = 1.f;

	// 마지막 스왑 뒤 이 시간 안에 카운터를 안 치면 빨간 거울에서 저스트 가드 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Shuffle")
	float CounterTimeLimit = 8.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Shuffle", meta = (ClampMin = "1"))
	int32 RequiredCounterSuccesses = 2;

	// 카운터 성공·저스트 가드가 끝나고 다음 라운드까지 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Shuffle")
	float RoundRestDuration = 1.5f;

	// 거울 섞기 동안 카운터 스킬 쿨타임 (초) — 오답 뒤 다음 라운드에 다시 칠 수 있게
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Shuffle")
	float CounterSkillCooldown = 5.f;

	// 저스트 가드 장판 반지름 (cm) — 거울 섞기 카메라(암 2000, Pitch -40) 한 화면에 원 전체가 들어오는 정도.
	// 판정은 위치와 무관(장판 밖으로 도망쳐도 맞음) — 크기는 보이는 것만
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Shuffle")
	float JustGuardRadius = 1300.f;

	// ── 보스 등장 ──
	// 마지막 카운터 성공 뒤 거울이 사라지고 보스가 나오기까지 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Emerge")
	float EmergeDelay = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Emerge")
	float MirrorVanishDuration = 0.8f;

	// ── 격돌 발판 ──
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Platform")
	TSubclassOf<AEchidnaClashPlatformActor> PlatformClass;

	// 보스 정면으로 이만큼 앞에 발판 (cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Platform")
	float PlatformFrontDistance = 350.f;

	// 이 시간(초) 안에 발판에서 G를 안 누르면 레이드 실패. 0 이하 = 무제한
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Platform")
	float PlatformTimeLimit = 20.f;

	// ── QTE 격돌 — 타이밍 1·2·3개 + 연타 (AEchidnaClashQTEActor) ──
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Clash")
	TSubclassOf<AEchidnaClashQTEActor> ClashQTEClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FinalPhase|Clash")
	float ClashCameraArmLength = 900.f;

private:
	enum class EShuffle : uint8 { Appear, Reveal, PrePass, PortraitPass, PassGap, PreSwap, Swapping, SwapGap, AwaitCounter, JustGuard, RoundRest, Emerge };

	void EnterStep(EEchidnaFinalPhaseStep NewStep);
	void TickCutscene();
	void TickPlatform();
	void TickClash();

	// ── 거울 섞기 ──
	void BeginShuffle();
	void TickShuffle(float DeltaTime);
	void SetShuffle(EShuffle NewSub);
	void StartRound();
	void DoSwap();
	void DoPortraitPass();
	void SetCameraTilt(bool bTilt);
	void OnMirrorCountered(AEchidnaShuffleMirrorActor* Mirror, AActor* Attacker);
	void StartJustGuard(int32 MirrorIndex);
	void EmergeBoss();
	void SetBossHidden(bool bInHidden);
	void SetAllMirrorsAcceptCounter(bool bAccept);
	void ClearMirrors(bool bAnimate);

	void SpawnPlatform(const FVector& Location);
	void OnPlatformInteracted(APawn* Pawn);
	void Finish(bool bSuccess, const FText& FailReason = FText::GetEmpty());
	void ReleasePlayer();
	void SetCameraZoom(float ArmLength);
	void ShowStepMessage(const FString& Message, float Duration = 3.f) const;

	TWeakObjectPtr<AEchidnaBoss> Boss;
	TWeakObjectPtr<ALoACharacter> Player;
	TWeakObjectPtr<AHexArena> Arena;

	UPROPERTY(Transient)
	TObjectPtr<AEchidnaClashPlatformActor> Platform;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AEchidnaShuffleMirrorActor>> OuterMirrors;

	UPROPERTY(Transient)
	TObjectPtr<AEchidnaShuffleMirrorActor> CenterMirror;

	UPROPERTY(Transient)
	TObjectPtr<AEchidnaJustGuardZoneActor> JustGuardZone;

	UPROPERTY(Transient)
	TObjectPtr<AEchidnaClashQTEActor> ClashQTE;

	EEchidnaFinalPhaseStep Step = EEchidnaFinalPhaseStep::None;
	float StepElapsed = 0.f;

	EShuffle Shuffle = EShuffle::Appear;
	float ShuffleElapsed = 0.f;
	FVector ArenaCenter = FVector::ZeroVector;		// 가운데 타일 윗면
	int32 RedIndex = INDEX_NONE;
	int32 SwapsDone = 0;
	int32 PassesDone = 0;
	int32 ShownCountdown = -1;
	int32 PassNet = 0;
	FString PassLog;
	int32 CounterSuccesses = 0;
	int32 EmergeMirrorIndex = INDEX_NONE;
	FIntPoint LastSwapPair = FIntPoint(INDEX_NONE, INDEX_NONE);

	bool bPlayerHeld = false;
	bool bCameraOverridden = false;
	bool bCameraTilted = false;
	bool bCounterCooldownOverridden = false;
	bool bBossHidden = false;
};
