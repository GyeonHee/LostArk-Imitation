#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "EchidnaClashQTEActor.generated.h"

class ALoACharacter;
class AEchidnaBoss;
class UClashQTEWidget;

/**
 * 0줄 발악 격돌 QTE — AEchidnaFinalPhaseActor가 Clash 단계에서 스폰하고 IsFinished/WasSuccess를 폴링한다.
 * 타이밍 단계 StageNoteCounts(1·2·3개) → 마지막 연타 단계(랜덤 키 두 개를 번갈아 눌러 게이지를 끝까지) = 총 7번.
 *  - 타이밍: 노트마다 링이 NoteApproachTime 동안 줄어 판정 시각에 키 원과 겹친다. ±HitWindow 안에 그 키 = 성공
 *    (±PerfectWindow = PERFECT). 다른 키·창 밖 입력·지나침 = MISS. MaxMisses를 넘으면 그 자리에서 실패
 *    판정 시각보다 EarlyIgnore 넘게 이른 입력은 무시(다음 노트를 미리 누른 것으로 치지 않음)
 *  - 연타: 다음 키를 맞게 누를 때마다 +MashGainPerPress, 매초 -MashDecayPerSecond. MashTimeLimit 안에 가득 = 성공
 * 키 입력은 ALoAPlayerController::InputKey가 HandleKey로 넘기고, 후보 키(Q W E R A S D F)는 스킬로 새지 않게 먹는다.
 * 박진감: 연타 게이지가 찰수록 카메라가 다가가고, 판정마다 큰 글자 + 플레이어 머리 위 라벨.
 * 실시간 진행(CustomTimeDilation 1)
 */
UCLASS(Blueprintable)
class LOA_API AEchidnaClashQTEActor : public AActor
{
	GENERATED_BODY()

public:
	AEchidnaClashQTEActor();

	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void Activate(ALoACharacter* InPlayer, AEchidnaBoss* InBoss);

	/** 키 입력 — QTE가 먹었으면 true(스킬 등으로 넘기지 않음) */
	bool HandleKey(const FKey& Key);

	bool IsAcceptingInput() const { return bActive && !bResolved; }
	bool IsFinished() const { return bFinished; }
	bool WasSuccess() const { return bSuccess; }

	// 랜덤 키 후보 (스킬 칸 Q~F와 같은 키)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashQTE")
	TArray<FKey> CandidateKeys;

	// 타이밍 단계별 노트 수 — 뒤에 연타 단계가 하나 더 붙는다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashQTE|Timing")
	TArray<int32> StageNoteCounts = { 1, 2, 3 };

	// 시작 전 "격돌!" 표시 시간 / 단계 사이 쉼 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashQTE|Timing")
	float StartDelay = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashQTE|Timing")
	float StageGap = 0.7f;

	// 링이 나타나 판정 시각까지 줄어드는 시간 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashQTE|Timing")
	float NoteApproachTime = 1.1f;

	// 한 단계 안 노트 사이 간격 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashQTE|Timing")
	float NoteInterval = 0.55f;

	// 판정 창 ± (초) — 저스트 가드와 같은 근거(입출력 지연 50~80ms + 사람 오차)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashQTE|Timing")
	float HitWindow = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashQTE|Timing")
	float PerfectWindow = 0.06f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashQTE|Timing")
	float EarlyIgnore = 0.35f;

	// 이 횟수를 넘게 놓치면 실패 (0 = 하나라도 놓치면 실패)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashQTE|Timing", meta = (ClampMin = "0"))
	int32 MaxMisses = 0;

	// ── 연타 ──
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashQTE|Mash")
	float MashTimeLimit = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashQTE|Mash")
	float MashStartGauge = 0.15f;

	// 맞는 키 한 번에 오르는 양 — 0.06이면 감소 없이 약 15번(번갈아 초당 6~7번이면 3초 안팎)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashQTE|Mash")
	float MashGainPerPress = 0.06f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashQTE|Mash")
	float MashDecayPerSecond = 0.18f;

	// 엉뚱한 키(같은 키 연속 포함)를 누르면 깎이는 양
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashQTE|Mash")
	float MashWrongKeyPenalty = 0.03f;

	// ── 연출 ──
	// 연타 게이지 0 → 1에 따라 카메라 암 길이 (가까워질수록 긴장감)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashQTE|Visual")
	float MashCameraArmStart = 900.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashQTE|Visual")
	float MashCameraArmEnd = 600.f;

	// 결과 글자를 보여주고 끝나기까지 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ClashQTE|Visual")
	float ResultLingerTime = 1.2f;

private:
	struct FQteNote
	{
		FKey Key;
		float HitTime = 0.f;	// Elapsed 기준 판정 시각
		int32 Result = 0;		// 0 대기, 1 Perfect, 2 Good, 3 Miss
	};

	void BeginStage(int32 StageIndex);
	void TickTimingStage();
	void TickMashStage(float DeltaTime);
	void ResolveNote(int32 Index, int32 Result);
	void Resolve(bool bInSuccess);
	FKey PickRandomKey(const FKey& Exclude = EKeys::Invalid) const;
	void ShowJudge(int32 Result);

	TWeakObjectPtr<ALoACharacter> Player;
	TWeakObjectPtr<AEchidnaBoss> Boss;

	UPROPERTY(Transient)
	TObjectPtr<UClashQTEWidget> Widget;

	TArray<FQteNote> Notes;
	int32 Stage = -1;			// -1 시작 전, 0..N-1 타이밍, N 연타
	float Elapsed = 0.f;
	float NextStageAt = 0.f;	// 단계 사이 쉼 — 이 시각에 다음 단계
	bool bWaitingNextStage = false;
	int32 Misses = 0;

	FKey MashKeyList[2];
	int32 MashNext = 0;
	float MashGauge = 0.f;
	float MashStart = 0.f;

	float ResolvedAt = 0.f;
	bool bActive = false;
	bool bResolved = false;
	bool bSuccess = false;
	bool bFinished = false;
};
