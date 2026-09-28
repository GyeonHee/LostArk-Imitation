// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
//#include "Templates/SubclassOf.h"
#include "GameFramework/PlayerController.h"
#include "LoAPlayerController.generated.h"

class UNiagaraSystem;
class UInputMappingContext;
class UInputAction;
class UPathFollowingComponent;
class USkillManagerComponent;
class UHUD_ViewModel;
class USkillTree_ViewModel;
class ALoACharacter;
class UBossHPWidget;
class UCastBarWidget;
class AEchidnaBoss;
class ACameraActor;
class UCinematicOverlayWidget;
class UDefeatWidget;
class UClearWidget;
class UAbandonRaidWidget;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  Player controller for a top-down perspective game.
 *  Implements point and click based controls
 */
UCLASS(abstract)
class ALoAPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:

	/** Component used for moving along a NavMesh path. */
	UPROPERTY(VisibleDefaultsOnly, Category = AI)
	TObjectPtr<UPathFollowingComponent> PathFollowingComponent;

	/** Time Threshold to know if it was a short press */
	UPROPERTY(EditAnywhere, Category="Input")
	float ShortPressThreshold;

	/** FX Class that we will spawn when clicking */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UNiagaraSystem> FXCursor;

	/** MappingContext */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** 스킬 전용 MappingContext */
	UPROPERTY(EditAnywhere, Category="Input|Skills")
	TObjectPtr<UInputMappingContext> SkillMappingContext;

	/** 스킬 슬롯 IA (인덱스 0=Q, 1=W, 2=E, 3=R, 4=A, 5=S, 6=D, 7=F) */
	UPROPERTY(EditAnywhere, Category="Input|Skills")
	TArray<TObjectPtr<UInputAction>> SkillSlotActions;

	/** 기본공격 IA (좌클릭 / C키) */
	UPROPERTY(EditAnywhere, Category="Input|Skills")
	TObjectPtr<UInputAction> BasicAttackAction;

	/** 대쉬 IA (스페이스바) */
	UPROPERTY(EditAnywhere, Category="Input|Skills")
	TObjectPtr<UInputAction> DashAction;

	
	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> SetDestinationClickAction;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> SetDestinationTouchAction;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> HUDWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	TObjectPtr<UUserWidget> HUDWidget;

	/** 보스 HP 바 위젯 클래스 — BP_LoAPlayerController에서 WBP_BossHP 할당.
	 *  레벨에 AEchidnaBoss가 없으면 위젯 자체를 만들지 않는다 */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UBossHPWidget> BossHPWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	TObjectPtr<UBossHPWidget> BossHPWidget;

	/** HP 갱신 때마다 남은 줄 수를 다시 물어보기 위해 들고 있는 참조 */
	TWeakObjectPtr<AEchidnaBoss> TrackedBoss;

	/** AEchidnaBoss::OnHPChanged 구독 콜백 — 위젯에 HP 비율과 줄 수를 넘긴다 */
	void OnBossHPChanged(double NewHP, double NewMaxHP);

	/** 캐스팅/차지 진행바 위젯 클래스 — BP_LoAPlayerController에서 WBP_CastBar 할당 */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UCastBarWidget> CastBarWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	TObjectPtr<UCastBarWidget> CastBarWidget;

	/** Tick에서 매 프레임 호출 — SkillManager를 폴링해 진행바를 갱신하거나 숨긴다 */
	void UpdateCastBar();

	// ── 미니맵 ──
	UPROPERTY(Transient)
	TObjectPtr<class UMinimapWidget> MinimapWidget;

	// ── 화면 연기 (랜잡 패턴) ──
	UPROPERTY(Transient)
	TObjectPtr<class UScreenFogWidget> ScreenFogWidget;

	float FogCurrentOpacity = 0.f;
	float FogTargetOpacity = 0.f;
	float FogFadeSpeed = 1.f;

	// Tick에서 연기 불투명도를 목표값으로 보간 — UpdateCastBar와 같이 IsActionLocked 조기 return보다 앞에서 호출
	void UpdateScreenFog(float DeltaSeconds);

public:
	/** 화면 전체 핑크 연기 켜기/끄기 — FadeTime초 동안 서서히. 위젯은 처음 켤 때 만들고 HUD 아래(ZOrder -1)에 깐다 */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetScreenFog(bool bEnable, float FadeTime = 1.f);

	/** 화면 상단 중앙 알림 띠를 Duration초 동안 띄운다 (예: 정비소 밖에서 스킬 등록 시도) — 다시 부르면 글자·시간만 갱신 */
	void ShowTimedNotice(const FText& Title, const FText& Message, float Duration = 2.f);

private:
	UPROPERTY(Transient)
	TObjectPtr<class UZoneNoticeWidget> TimedNoticeWidget;

	FTimerHandle TimedNoticeTimer;

	// ── 보스 맵 진입 인트로 ──────────────────────────────────
	// 보스(bWaitForIntro)가 있는 레벨에 들어오면: 검은 화면 페이드 인 → 보스 정면 높은 곳에서 아레나 전체를 비추다
	// 보스 얼굴 쪽으로 다가감(IntroCameraDuration) + 레터박스·보스 이름 → 플레이어 카메라로 블렌드(IntroBlendOutTime) → AEchidnaBoss::StartCombat.
	// 그동안 플레이어는 붙잡힘(조작 불가), HUD·보스 HP·미니맵·캐스팅 바는 숨김, K 스킬창도 막는다
public:
	UFUNCTION(BlueprintPure, Category = "RaidIntro")
	bool IsRaidIntroActive() const { return bRaidIntroActive; }

protected:
	UPROPERTY(EditDefaultsOnly, Category = "RaidIntro")
	float IntroCameraDuration = 2.5f;

	UPROPERTY(EditDefaultsOnly, Category = "RaidIntro")
	float IntroBlendOutTime = 0.7f;

	UPROPERTY(EditDefaultsOnly, Category = "RaidIntro")
	float IntroFadeInTime = 0.6f;

	// 카메라 시작/끝 위치 — 보스 기준 로컬 오프셋 (X = 보스 정면, Y = 오른쪽, Z = 위). 시작은 높고 멀리(아레나 전체), 끝은 보스 얼굴 앞
	UPROPERTY(EditDefaultsOnly, Category = "RaidIntro")
	FVector IntroStartOffset = FVector(2600.f, 0.f, 2200.f);

	UPROPERTY(EditDefaultsOnly, Category = "RaidIntro")
	FVector IntroEndOffset = FVector(800.f, 250.f, 200.f);

	// 카메라가 바라보는 지점 — 보스 캡슐 중심에서 이만큼 위 (얼굴 근처)
	UPROPERTY(EditDefaultsOnly, Category = "RaidIntro")
	float IntroLookAtHeight = 120.f;

	UPROPERTY(EditDefaultsOnly, Category = "RaidIntro")
	FText IntroTitle = NSLOCTEXT("RaidIntro", "Title", "에키드나");

	UPROPERTY(EditDefaultsOnly, Category = "RaidIntro")
	FText IntroSubtitle = NSLOCTEXT("RaidIntro", "Subtitle", "2관문");

private:
	void BeginRaidIntro(AEchidnaBoss* Boss);
	void UpdateRaidIntro(float DeltaSeconds);
	void EndRaidIntro();

	bool bRaidIntroActive = false;
	bool bIntroBlendingOut = false;
	float RaidIntroElapsed = 0.f;

	TWeakObjectPtr<AEchidnaBoss> IntroBoss;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> IntroCamera;

	UPROPERTY(Transient)
	TObjectPtr<UCinematicOverlayWidget> IntroOverlay;

	// 인트로 동안 숨긴 HUD 위젯들과 원래 Visibility
	UPROPERTY(Transient)
	TArray<TObjectPtr<UUserWidget>> IntroHiddenWidgets;

	TArray<ESlateVisibility> IntroHiddenVisibilities;

	// ── 사망 / 공략 실패 ──────────────────────────────────────
	// 플레이어가 죽으면(ALoACharacter::OnDied): 화면이 DefeatGrayFadeTime에 걸쳐 흑백이 되고 패배 화면(UDefeatWidget)이 뜬 뒤
	// DefeatReturnDelay초 후 RaidReturnLevel(대기 지역)로 이동 — 거기서 정비소 위치에 새 캐릭터로 선다
protected:
	UPROPERTY(EditDefaultsOnly, Category = "Defeat")
	float DefeatReturnDelay = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "Defeat")
	float DefeatGrayFadeTime = 0.6f;

	// 사망·클리어 후 돌아갈 대기 지역
	UPROPERTY(EditDefaultsOnly, Category = "Defeat")
	TSoftObjectPtr<UWorld> RaidReturnLevel = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/LostArk/Level/Lv_EchidnaLobby.Lv_EchidnaLobby")));

	UPROPERTY(EditDefaultsOnly, Category = "Defeat")
	FText DeathCauseText = NSLOCTEXT("Defeat", "DeathCause", "플레이어님이 욕망의 주인, 에키드나에 의해 사망하였습니다.");

private:
	void OnPlayerDied();
	void UpdateDefeat(float DeltaSeconds);

	// 대기 지역에 막 도착했고 사망 복귀 중이면 폰을 정비소 위치로 옮긴다 (BeginPlay·OnPossess 둘 다에서 — 빙의 순서가 달라서)
	void MoveToRepairStationIfReturning(APawn* InPawn);

	bool bDefeatActive = false;
	bool bDefeatTravelling = false;
	float DefeatElapsed = 0.f;
	int32 DefeatShownSeconds = -1;

	UPROPERTY(Transient)
	TObjectPtr<UDefeatWidget> DefeatWidget;

	// ── 보스 처치 / 던전 클리어 ────────────────────────────────
	// 보스 HP 0(AEchidnaBoss::OnDefeated): 플레이어 무적 + 클리어 화면(UClearWidget, 흑백 아님) → ClearReturnDelay초 뒤 정비소 복귀
protected:
	UPROPERTY(EditDefaultsOnly, Category = "Clear")
	float ClearReturnDelay = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "Clear")
	FText ClearHeadlineText = NSLOCTEXT("Clear", "Headline", "욕망의 주인, 에키드나를 처치하였습니다.");

private:
	void OnBossDefeated();
	void UpdateClear(float DeltaSeconds);

	// 사망·클리어 공통 — 정비소 복귀 표시 후 대기 지역으로
	void ReturnToLobby();

	bool bClearActive = false;
	bool bClearTravelling = false;
	float ClearElapsed = 0.f;
	int32 ClearShownSeconds = -1;

	UPROPERTY(Transient)
	TObjectPtr<UClearWidget> ClearWidget;

	// ── 레이드 중단 / 레벨 이동 페이드 / 테스트 치트 ────────────────────
public:
	/** 레이드 포기 — 보스 맵 좌상단 "중단하기" 확인 시. 무적+정지 후 정비소로 복귀 */
	void AbandonRaid();

	/** 화면을 검게 페이드 아웃한 뒤 레벨 이동 — 도착한 레벨은 BeginPlay에서 페이드 인(인트로가 있으면 인트로가 담당).
	 *  보스 입장·사망·클리어·중단 모두 이걸로 옮긴다. 이동 대기 중 중복 호출은 무시 */
	void TravelToLevelWithFade(const TSoftObjectPtr<UWorld>& Level);

	/** 테스트용 콘솔 명령 — 보스 즉사(클리어 확인용) */
	UFUNCTION(Exec)
	void LoAKillBoss();

	/** 테스트용 콘솔 명령 — 플레이어 즉사(사망 확인용) */
	UFUNCTION(Exec)
	void LoAKillSelf();

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Travel")
	float TravelFadeOutTime = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Travel")
	float ArriveFadeInTime = 0.5f;

private:
	// 이번 판에 치트를 썼으면 클리어해도 최고 기록에 남기지 않는다
	bool bCheatUsedThisRaid = false;

	bool bLevelTravelPending = false;
	TSoftObjectPtr<UWorld> PendingTravelLevel;
	FTimerHandle TravelFadeTimer;

	UPROPERTY(Transient)
	TObjectPtr<UAbandonRaidWidget> AbandonWidget;

protected:
	// 보스 HP 바 옆 광폭화 타이머 + 초상화 아래 정산 게이지 갱신 — UpdateCastBar와 같은 이유로 Tick의 IsActionLocked 조기 return보다 앞에서 호출
	void UpdateEnrageTimer();

	UPROPERTY(BlueprintReadOnly, Category = "UI")
	TObjectPtr<UHUD_ViewModel> HUDViewModel;

	/** K 키 — 스킬트리 토글 */
	UPROPERTY(EditAnywhere, Category="Input|UI")
	TObjectPtr<UInputAction> SkillTreeAction;

	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UUserWidget> SkillTreeWidgetClass;

	UPROPERTY()
	TObjectPtr<UUserWidget> SkillTreeWidget;

	UPROPERTY(BlueprintReadOnly, Category="UI")
	TObjectPtr<USkillTree_ViewModel> SkillTreeViewModel;

	/** True if the controlled character should navigate to the mouse cursor. */
	uint32 bMoveToMouseCursor : 1;

	/** Set to true if we're using touch input */
	uint32 bIsTouch : 1;

	/** Saved location of the character movement destination */
	FVector CachedDestination;

	/** Time that the click input has been pressed */
	float FollowTime = 0.0f;

	/** True while auto-moving to a clicked destination */
	bool bAutoMoving = false;

	/** True while right-click is held down */
	bool bHoldMoving = false;

	/** True after StopMovement (dash) until velocity returns to normal */
	bool bDashSuppressed = false;

	/** Minimum frames to keep suppressed (prevents instant clear while PendingLaunchVelocity not yet applied) */
	int32 DashSuppressFrames = 0;

	/** Whether auto-move was active before the dash — restored after dash ends */
	bool bWasAutoMovingBeforeDash = false;

	/** 기본공격·스킬로 멈춘 뒤 새 이동 클릭(OnInputStarted)이 들어오기 전까지 true — 이동 버튼을 누른 채였어도 다시 걷지 않게 한다 */
	bool bMoveHaltedByAttack = false;

	/** 스킬트리 토글 중복 호출 방지용 타임스탬프 */
	float LastSkillTreeToggleTime = -1.f;

	/** Distance to destination at which movement stops */
	UPROPERTY(EditAnywhere, Category="Movement", meta=(ClampMin=0, Units="cm"))
	float AutoMoveAcceptanceRadius = 20.0f;

	/** 매혹(3스택) 상태 동안 무작위 이동/스킬 사용을 반복 실행하는 타이머 — 시작/정지는 OnPlayerCharmedChanged에서 관리 */
	FTimerHandle CharmConfusionTimerHandle;

	/** 매혹 중 무작위로 누른 스킬 슬롯을 짧게 뗄 때 쓰는 타이머 */
	FTimerHandle CharmSkillReleaseTimerHandle;

	/** 매혹 중 현재 붙잡고 있는 스킬 슬롯 (-1=없음) */
	int32 CharmActiveSkillSlot = -1;

	/** 매혹 중 무작위 행동(이동 목표 재설정 + 쓸 수 있는 스킬 소모)을 반복하는 간격 (초).
	 *  짧을수록 이동이 산만해지고 쿨이 도는 족족 스킬이 빠져나간다 */
	UPROPERTY(EditAnywhere, Category="Charm")
	float CharmActionInterval = 0.8f;

	/** 매혹 중 무작위 이동 목표 지점을 고를 반경 (cm) */
	UPROPERTY(EditAnywhere, Category="Charm")
	float CharmWanderRadius = 400.f;

public:

	/** Constructor */
	ALoAPlayerController();

	// 스킬 등 외부에서 강제 이동 목표 설정
	void ForceMoveTo(const FVector& Destination);
	void CancelAutoMove();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void OnPossess(APawn* InPawn) override;

	virtual void StopMovement() override;

protected:

	/** Initialize input bindings */
	virtual void SetupInputComponent() override;
	
	/** Input handlers */
	void OnInputStarted();
	void OnSetDestinationTriggered();
	void OnSetDestinationReleased();
	void OnTouchTriggered();
	void OnTouchReleased();

	/** Helper function to get the move destination */
	void UpdateCachedDestination();

	/** 스킬 입력 핸들러 */
	void OnSkillKeyDown(int32 SlotIndex);
	void OnSkillKeyHeld(int32 SlotIndex);
	void OnSkillKeyUp(int32 SlotIndex);

	/** 대쉬 입력 핸들러 */
	void OnDashInput();

	/** 현재 빙의된 캐릭터의 SkillManager 반환 */
	UFUNCTION(BlueprintCallable, Category="Skills")
	USkillManagerComponent* GetSkillManager() const;

	UFUNCTION(BlueprintCallable, Category="UI")
	void OnSkillTreeToggle();

	void OnPlayerHPChanged(float NewHP);
	void OnPlayerMPChanged(float NewMP);

	/** 매혹 상태(3스택) 전환 시 호출 — 시작되면 무작위 이동/스킬 사용 타이머 시작, 끝나면 정지하고 붙잡고 있던 스킬 키를 뗌 */
	void OnPlayerCharmedChanged(bool bCharmed);

	/** 매혹 중 CharmActionInterval마다 호출 — 무작위 지점으로 이동 목표를 다시 잡고,
	 *  쿨타임이 돌아 쓸 수 있는 슬롯(0~7) 중 하나를 골라 타입별 필요 시간만큼 붙잡아 실제로 발동시킨다 */
	void PerformRandomCharmAction();

	/** 매혹 중 붙잡고 있던 스킬 슬롯을 떼는 콜백 — 떼는 순간 다음 틱에서 새 스킬을 고를 수 있게 된다 */
	void ReleaseCharmSkill();

	/** 캐릭터의 HP/MP 변경 델리게이트에 바인딩하고 ViewModel 초기화 */
	void BindCharacterEvents(ALoACharacter* InCharacter);
};


