// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
//#include "Templates/SubclassOf.h"
#include "GameFramework/PlayerController.h"
#include "Item/RaidReward.h"
#include "LoAPlayerController.generated.h"

enum class EEquipSlot : uint8;

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

	// 배틀아이템 원형 쿨타임 — 스킬 칸과 같은 머티리얼(CooldownRatio = 남은 비율)
	UPROPERTY(EditDefaultsOnly, Category = "UI|BattleItem")
	TSoftObjectPtr<class UMaterialInterface> BattleItemCooldownMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/LostArk/Material/M_CooldownRadial.M_CooldownRadial")));

	// 다 쓴 아이템 아이콘 색 (빨간 반투명)
	UPROPERTY(EditDefaultsOnly, Category = "UI|BattleItem")
	FLinearColor BattleItemDepletedTint = FLinearColor(1.f, 0.12f, 0.12f, 0.5f);

	// ── 미니맵 ──
	UPROPERTY(Transient)
	TObjectPtr<class UMinimapWidget> MinimapWidget;

	// 좌상단 자원 띠 (현재 시간 · 실링 · 골드 · 운명의 파편) — 모든 맵
	UPROPERTY()
	TObjectPtr<class UResourceBarWidget> ResourceBarWidget;

	// 소서리스 아이덴티티 게이지 (스킬 칸 위) — 소서리스가 아니면 위젯이 스스로 숨는다
	UPROPERTY(Transient)
	TObjectPtr<class UIdentityGaugeWidget> IdentityGaugeWidget;

	// ── 화면 연기 (랜잡 패턴) ──
	UPROPERTY(Transient)
	TObjectPtr<class UScreenFogWidget> ScreenFogWidget;

	float FogCurrentOpacity = 0.f;
	float FogTargetOpacity = 0.f;
	float FogFadeSpeed = 1.f;

	// 연기를 켜둔 출처들 — 패턴(랜잡·그네)과 매혹 상태가 서로의 연기를 끄지 않도록, 하나라도 남아 있으면 연기 유지
	TSet<FName> ScreenFogSources;

	// Tick에서 연기 불투명도를 목표값으로 보간 — UpdateCastBar와 같이 IsActionLocked 조기 return보다 앞에서 호출
	void UpdateScreenFog(float DeltaSeconds);

public:
	/** 화면 전체 핑크 연기 켜기/끄기 — FadeTime초 동안 서서히. 위젯은 처음 켤 때 만들고 HUD 아래(ZOrder -1)에 깐다 */
	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetScreenFog(bool bEnable, float FadeTime = 1.f);

	/** 출처별 연기 요청 — SetScreenFog는 "Pattern" 출처. 매혹 상태는 "Charm" 출처로 켜고 끈다 */
	void SetScreenFogSource(FName Source, bool bEnable, float FadeTime = 1.f);

	/** 보스 대사창(하단 중앙, 초상화 + 이름 + 대사)을 페이드 인 — HideBossDialogue로 내린다 */
	void ShowBossDialogue(const FText& Speaker, const FText& Line, class UTexture2D* Portrait);
	void HideBossDialogue();

	/** 화면 상단 중앙 알림 띠를 Duration초 동안 띄운다 (예: 정비소 밖에서 스킬 등록 시도) — 다시 부르면 글자·시간만 갱신 */
	void ShowTimedNotice(const FText& Title, const FText& Message, float Duration = 2.f);

private:
	UPROPERTY(Transient)
	TObjectPtr<class UZoneNoticeWidget> TimedNoticeWidget;

	// ── HUD 슬롯 클릭·드래그 스왑 ──────────────────────────────
	// HUD 루트가 HitTestInvisible(클릭 이동을 막지 않으려고)이라 슬롯 위젯은 마우스를 못 받는다 — 그래서 컨트롤러가
	// 커서 위치와 슬롯 위젯 Geometry를 직접 비교한다. 슬롯 위에서 누른 클릭은 기본공격·이동으로 넘기지 않고,
	// 왼쪽 버튼이면 드래그를 시작해 같은 종류 슬롯 위에서 놓으면 맞바꾼다.
	// 그룹: 0 스킬(Q~F) / 1 아이템(F1,5~9) / 2 배틀아이템(1~4) / 3 기타(대시·기상 — 클릭만 막고 드래그 없음)

	/** 커서 아래 HUD 슬롯 — 없으면 false. OutIndex는 그룹 안 순번(스킬은 곧 스킬 슬롯 번호) */
	bool FindHUDSlotUnderCursor(int32& OutGroup, int32& OutIndex) const;

	/** 마우스 버튼 입력이 HUD 슬롯에서 시작됐으면 true(게임 입력으로 쓰지 말 것). 왼쪽 버튼이면 드래그 준비 */
	bool ConsumeHUDSlotPress();

	void UpdateSlotDrag();
	void FinishSlotDrag();
	void SwapHUDSlots(int32 Group, int32 IndexA, int32 IndexB);
	bool GetHUDSlotIconBrush(int32 Group, int32 Index, FSlateBrush& OutBrush) const;

	// ── 배틀아이템 칸 1~4 (키 1~4, HUD Border_1~4) ──
	void OnBattleItemKey(int32 SlotIndex);

	/** WBP_HUD의 Border_1~4 안(Img_1~4 하나뿐)을 C++로 Overlay[아이콘, 원형 쿨타임, 쿨타임 초, 개수]로 감싼다 — WBP 수정 불필요 */
	void BuildBattleItemHUD();
	void UpdateBattleItemHUD();

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UImage>> BattleItemIcons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UImage>> BattleItemCooldownImages;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UMaterialInstanceDynamic>> BattleItemCooldownMIDs;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UTextBlock>> BattleItemCooldownTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UTextBlock>> BattleItemCountTexts;

	bool bSlotPressActive = false;
	bool bSlotDragging = false;
	int32 SlotPressGroup = -1;
	int32 SlotPressIndex = -1;
	FVector2D SlotPressMouse = FVector2D::ZeroVector;

	// 슬롯 위에서 시작된 기본공격 입력 — Held/Up도 같이 버린다
	bool bBasicAttackSuppressed = false;

	// 이동 클릭이 HUD 슬롯·소지품 창 위에서 시작됨 — 누르고 있는 동안(Triggered)·뗄 때(Released) 이동도 하지 않는다
	bool bMovePressSuppressed = false;

	UPROPERTY(Transient)
	TObjectPtr<class USlotDragVisualWidget> SlotDragVisual;

	UPROPERTY(Transient)
	TObjectPtr<class UBossDialogueWidget> BossDialogueWidget;

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
	// 클리어 화면이 떠 있는 시간 — 전리품을 보고 [더보기]를 고를 여유. [정비소로 이동]으로 바로 넘어갈 수 있다
	UPROPERTY(EditDefaultsOnly, Category = "Clear")
	float ClearReturnDelay = 12.f;

	UPROPERTY(EditDefaultsOnly, Category = "Clear")
	FText ClearHeadlineText = NSLOCTEXT("Clear", "Headline", "욕망의 주인, 에키드나를 처치하였습니다.");

public:
	/** 클리어 화면 [더보기] — 실링을 내고 재료 +50% (한 번만) */
	void RequestClearBonus();
	/** 클리어 화면 [정비소로 이동] — 카운트다운을 기다리지 않고 복귀 */
	void RequestClearLeave();

private:
	void OnBossDefeated();
	void UpdateClear(float DeltaSeconds);

	// 대기 보상(GameInstance::PendingRewards)을 지금 소지품에 넣고 저장. 반환 = 아직 남은 줄 수
	int32 DeliverPendingRewardsNow(TArray<FLoARewardItem>& OutDelivered);
	// 대기 지역 — 소지품이 가득 차 못 받은 보상을 주기적으로 다시 넣어 본다
	void TryDeliverPendingRewards();
	void RefreshClearRewardWidget();
	FTimerHandle PendingRewardTimer;

	TArray<FLoARewardItem> ClearRewards;   // 이번 클리어에 받은 보상 (더보기 포함, 화면 표시용)
	bool bClearFirstReward = false;
	bool bClearBonusTaken = false;
	bool bClearRewardLeftover = false;            // 소지품이 가득 차 일부가 대기 보상으로 남음

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

	/** 테스트용 콘솔 명령 — 소서리스 신비한 마력 게이지 설정(0~100). 예) LoAIdentity 100 */
	UFUNCTION(Exec)
	void LoAIdentity(float Gauge);

	/** 테스트용 콘솔 명령 — 소지품에 아이템 추가. 예) LoAGiveItem SpiritRecoveryPotion 50 */
	UFUNCTION(Exec)
	void LoAGiveItem(FName ItemId, int32 Count);

	/** 테스트용 콘솔 명령 — 회복약 묶음 여러 개를 빈 칸 아무 데나 흩뿌림(정렬·잠금 확인용) */
	UFUNCTION(Exec)
	void LoAInvTest();

	/** 테스트용 콘솔 명령 — 실링 추가(음수면 차감). 예) LoAGiveSilling 5000 */
	UFUNCTION(Exec)
	void LoAGiveSilling(int64 Amount);

	/** 테스트용 콘솔 명령 — 골드 추가(음수면 차감). 예) LoAGiveGold 50000 */
	UFUNCTION(Exec)
	void LoAGiveGold(int64 Amount);

	/** 테스트용 콘솔 명령 — 현재 스탯(기본 스탯·전투 특성·계산 결과)을 로그로 */
	UFUNCTION(Exec)
	void LoAStats();

	/** 테스트용 콘솔 명령 — 스탯 임시 가산(같은 대상은 덮어씀, 0이면 제거). 대상 이름은 EStatModifierTarget.
	 *  예) LoASetStat Crit 500 / LoASetStat Swiftness 800 / LoASetStat CritRate 0.5 / LoASetStat Reset */
	UFUNCTION(Exec)
	void LoASetStat(const FString& Target, double Value);

	/** 테스트용 콘솔 명령 — 데미지 미터를 비우고 Seconds초 뒤 결과를 로그(CSV)로. 예) LoADpsTest 60 */
	UFUNCTION(Exec)
	void LoADpsTest(float Seconds);

	/** 테스트용 콘솔 명령 — 장착 장비 재련 단계를 바로 설정(저장됨). 부위 이름은 EEquipSlot 또는 All.
	 *  예) LoASetRefine All 15 / LoASetRefine Weapon 20 */
	UFUNCTION(Exec)
	void LoASetRefine(const FString& Slot, int32 RefineLevel);

	/** 테스트용 콘솔 명령 — TargetLevel 단계를 Runs번 시뮬레이션해 평균·최악 시도 횟수 (확률표 검증). 예) LoARefineSim 25 10000 */
	UFUNCTION(Exec)
	void LoARefineSim(int32 TargetLevel, int32 Runs);

	// ── 소지품 창 (I키) ──
	void SetInventoryOpen(bool bOpen);
	bool IsInventoryOpen() const;

	// ── 캐릭터 정보 창 (P키) ──
	void SetCharacterInfoOpen(bool bOpen);
	bool IsCharacterInfoOpen() const;

	// ── 보석 합성 창 (소지품 아래 띠의 보석 버튼, 7단계) ──
	void SetGemFuseOpen(bool bOpen);
	bool IsGemFuseOpen() const;

	// ── 각인 효과 창 (P창 [각인] 탭의 [각인 효과 설정하기], 7단계) ──
	void SetEngravingSetupOpen(bool bOpen);
	bool IsEngravingSetupOpen() const;

	// ── 아크 패시브 창 ('.' 키 또는 P창의 [아크 패시브] 칸) ──
	void SetArkPassiveOpen(bool bOpen);
	bool IsArkPassiveOpen() const;

	// ── 데미지 미터 창 (L키) ──
	void SetDamageMeterOpen(bool bOpen);
	bool IsDamageMeterOpen() const;

	/** 소지품 칸을 창 밖에 놓았을 때(UInventoryWidget) — 커서가 HUD 배틀아이템 칸 위면 그 칸에 등록 */
	void HandleInventoryDropOutside(int32 InventoryIndex);

	// ── 대기 지역 NPC (상점·재련) ──
	/** NPC 메뉴 열기 — 공통(붙잡힘·카메라 줌인·HUD 숨김·하단 대화창) 후 NPC 종류에 맞는 창:
	 *  상점 = 왼쪽 상점 창 + 오른쪽 소지품 / 재련 = 가운데 재련 창. 닫을 때까지 캐릭터는 붙잡힘 */
	void OpenNPCMenu(class AInteractableNPCActor* NPC);
	/** NPC 메뉴 닫기 (대화창 [나가기]) — 카메라·HUD 원래대로 */
	void CloseNPCMenu();
	bool IsNPCMenuOpen() const { return bNPCMenuOpen; }
	bool IsShopOpen() const;

	/** 소지품 칸 우클릭 — 상점이 열려 있으면 판매 목록에 올림(Shift = 묶음 판매). 상점 밖에선 아직 아무것도 안 함 */
	void HandleInventorySlotRightClick(int32 InventoryIndex, bool bShift);

	/** 캐릭터 정보 창의 장착 칸 우클릭 — 해제해서 소지품으로 */
	void HandleEquipSlotRightClick(EEquipSlot Slot);

	int32 LastEquipClickIndex = INDEX_NONE;
	double LastEquipClickTime = -1.0;

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

	// 소지품 창 — WBP 없이 C++ 클래스로 생성 (ZOrder 9: 알림(8) 위, 스킬창(10) 아래)
	UPROPERTY(Transient)
	TObjectPtr<class UInventoryWidget> InventoryWidget;

	void OnInventoryToggle();

	// 캐릭터 정보 창(장비·스탯) — WBP 없이 C++ 클래스로 생성 (ZOrder 9, 소지품보다 먼저 만들어 소지품이 위에 그려짐)
	UPROPERTY(Transient)
	TObjectPtr<class UCharacterInfoWidget> CharacterInfoWidget;

	// 캐릭터 정보 창 가운데 3D 미리보기 — 맵 위 허공에 세워두고 창이 열려 있을 때만 캡처
	UPROPERTY(Transient)
	TObjectPtr<class ACharacterPreviewActor> CharacterPreviewActor;

	void OnCharacterInfoToggle();

	/** '.' 키 — 아크 패시브 창 */
	void OnArkPassiveToggle();

	// 스킬 툴팁 — HUD 스킬 칸·스킬트리 줄 위에 마우스를 올리면 (ZOrder 50, 맨 위)
	UPROPERTY(Transient)
	TObjectPtr<class USkillTooltipWidget> SkillTooltipWidget;

	void UpdateSkillTooltip();

	/** 스킬트리 창 목록에서 커서 아래 줄의 스킬 행 이름 (없으면 None) */
	FName FindSkillTreeRowUnderCursor() const;

	// 보석 합성 창 — C++ 트리 (ZOrder 10)
	UPROPERTY(Transient)
	TObjectPtr<class UGemFuseWidget> GemFuseWidget;

	// 어빌리티 스톤 세공 창 — [어빌리티 스톤 세공사] NPC (2026-10-25)
	UPROPERTY(Transient)
	TObjectPtr<class UStoneFacetWidget> StoneFacetWidget;

	// 팔찌 효과 부여 창 — [팔찌 효과 부여] NPC (2026-10-25)
	UPROPERTY(Transient)
	TObjectPtr<class UBraceletEnchantWidget> BraceletEnchantWidget;

	// 각인 효과 창 — C++ 트리 (ZOrder 10, 캐릭터 정보 창 위)
	UPROPERTY(Transient)
	TObjectPtr<class UEngravingSetupWidget> EngravingSetupWidget;

	// 아크 패시브 창 — C++ 트리 (ZOrder 10)
	UPROPERTY(Transient)
	TObjectPtr<class UArkPassiveWidget> ArkPassiveWidget;

	// 보스·허수아비에 준 피해 집계 — 피격 쪽이 ULoADamageLibrary::ReportDamageDealt로 기록
	UPROPERTY(VisibleAnywhere, Category="DamageMeter")
	TObjectPtr<class UDamageMeterComponent> DamageMeter;

	// 데미지 미터 창 — WBP 없이 C++ 클래스로 생성
	UPROPERTY(Transient)
	TObjectPtr<class UDamageMeterWidget> DamageMeterWidget;

	void OnDamageMeterToggle();

	/** 미터 기록이 바뀔 때 — 허수아비 세션이 처음 시작되면 창을 자동으로 연다(그 레벨에서 한 번) */
	void HandleDamageMeterChanged();
	bool bDamageMeterAutoOpened = false;

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

	/** 커서가 가리키는 조준 지점 — 먼저 Visibility 트레이스, 못 맞히면(맵 밖·하늘) 캐릭터 발밑 높이 평면과 교차,
	 *  그것도 안 되면(수평선 위) 커서 방향으로 1000cm. 방향만 필요한 스킬(돌풍)이 조준을 놓쳐 씹히지 않게 */
	bool GetCursorAimLocation(FVector& OutLocation, bool* bOutHitGround = nullptr) const;

	/** 장판 스킬용 목표 지점 — 바닥을 맞혔으면 그 지점(사거리 밖이면 스킬이 자동이동), 못 맞혔으면(맵 밖·하늘)
	 *  조준 방향으로 MaxRange 안까지 당긴 발밑 높이 지점. 허공으로 자동이동하면 벽에 막혀 영영 도착 못 하므로 */
	bool GetCursorGroundTarget(const AActor* Caster, float MaxRange, FVector& OutLocation) const;

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

	/** 소서리스 아이덴티티 — Z 마력 방출 / X 점멸 (입력 액션 없이 키 직접 바인딩) */
	void OnIdentityReleaseKey();
	void OnIdentityBlinkKey();
	/** 아이덴티티 입력을 받을 수 있는 상태인지 (CC·매혹 중이면 nullptr) */
	class ASorceress* GetIdentityInputPawn() const;

	/** 현재 빙의된 캐릭터의 SkillManager 반환 */
	UFUNCTION(BlueprintCallable, Category="Skills")
	USkillManagerComponent* GetSkillManager() const;

	UFUNCTION(BlueprintCallable, Category="UI")
	void OnSkillTreeToggle();

	// 상점 NPC — 거리 안의 NPC에 [G] 대화하기 + 테두리, 커서를 올린 NPC도 테두리 (Tick)
	void UpdateNPCInteraction();
	void OnInteractKey();

	TWeakObjectPtr<class AInteractableNPCActor> InRangeNPC;   // 상호작용 거리 안 가장 가까운 NPC
	TWeakObjectPtr<class AInteractableNPCActor> HoveredNPC;   // 커서 아래 NPC
	TWeakObjectPtr<class AInteractableNPCActor> ActiveNPC;    // 메뉴를 연 NPC
	bool bNPCMenuOpen = false;
	uint8 HUDVisibilityBeforeNPCMenu = 0;  // ESlateVisibility (헤더에 UMG 타입 include 안 하려고 정수로)

	UPROPERTY(Transient)
	TObjectPtr<class UShopWidget> ShopWidget;

	UPROPERTY(Transient)
	TObjectPtr<class URefineWidget> RefineWidget;

	UPROPERTY(Transient)
	TObjectPtr<class UAccessoryPolishWidget> PolishWidget;

	UPROPERTY(Transient)
	TObjectPtr<class UShopDialogueWidget> ShopDialogueWidget;

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


