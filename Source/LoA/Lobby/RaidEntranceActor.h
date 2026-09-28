#pragma once

#include "CoreMinimal.h"
#include "Lobby/LobbyZoneActor.h"
#include "RaidEntranceActor.generated.h"

/**
 * 보스 입장 구역 — 대기 지역의 팔각형 테두리 구역.
 * 안에 서 있으면 상단에 "잠시 후 다음 지역으로 이동됩니다. N초" 카운트다운(EnterCountdown, 3초)이 돈다.
 *  - 끝나기 전에 밖으로 나가면 카운트다운 초기화 + 알림 즉시 사라짐 (다시 들어오면 처음부터)
 *  - 0초가 되면 이동 확정 — 테두리가 초록색(ReadyBorderColor)으로 바뀌고 TravelDelay 뒤 TargetLevel로 이동.
 *    확정 뒤에는 나가도 취소되지 않는다
 */
UCLASS(Blueprintable)
class LOA_API ARaidEntranceActor : public ALobbyZoneActor
{
	GENERATED_BODY()

public:
	ARaidEntranceActor();

	// 이동할 보스 레벨
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entrance")
	TSoftObjectPtr<UWorld> TargetLevel;

	// 구역 안에 머물러야 하는 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entrance", meta = (ClampMin = "0"))
	float EnterCountdown = 3.f;

	// 카운트다운이 끝나 테두리가 초록으로 바뀐 뒤 실제로 레벨을 여는 데까지 (초) — 이동한다는 걸 눈으로 보여주는 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entrance", meta = (ClampMin = "0"))
	float TravelDelay = 0.8f;

	// 이동 확정 시 테두리 색 (HDR — 1보다 크면 발광)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entrance")
	FLinearColor ReadyBorderColor = FLinearColor(0.3f, 6.f, 0.6f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entrance")
	FText NoticeTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entrance")
	FText NoticeMessage;

protected:
	virtual void OnPlayerEnterZone(APawn* Player) override;
	virtual void OnPlayerExitZone(APawn* Player) override;
	virtual void TickPlayerInZone(APawn* Player, float DeltaSeconds) override;

private:
	void UpdateCounterText();
	void BeginTravel();
	void TravelToTarget();

	float Remaining = 0.f;
	int32 ShownSeconds = -1;

	// 카운트다운이 끝나 이동이 확정된 상태 (초록 테두리 ~ 레벨 전환)
	bool bTravelling = false;

	FTimerHandle TravelTimer;
};
