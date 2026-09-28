#pragma once

#include "CoreMinimal.h"
#include "Lobby/LobbyZoneActor.h"
#include "RepairStationActor.generated.h"

/**
 * 정비소 — 대기 지역의 동그란 테두리 구역.
 * 들어오는 순간 HP·MP를 가득 채우고 모든 스킬 쿨타임을 초기화한 뒤 상단에 "정비 완료" 알림을 잠깐 띄운다.
 * 나갔다가 다시 들어오면 또 정비된다.
 * 스킬 슬롯 등록(스킬트리 → Q~F 드래그)은 정비소 안에 있는 동안만 가능하다.
 */
UCLASS(Blueprintable)
class LOA_API ARepairStationActor : public ALobbyZoneActor
{
	GENERATED_BODY()

public:
	ARepairStationActor();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Repair")
	FText NoticeTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Repair")
	FText NoticeMessage;

	// 알림이 떠 있는 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Repair")
	float NoticeDuration = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Repair")
	bool bResetSkillCooldowns = true;

protected:
	virtual void OnPlayerEnterZone(APawn* Player) override;
	virtual void OnPlayerExitZone(APawn* Player) override;

private:
	FTimerHandle NoticeTimer;
};
