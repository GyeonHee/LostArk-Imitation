#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "LoAGameInstance.generated.h"

/**
 * 레벨이 바뀌어도 유지해야 하는 플레이어 데이터 보관소.
 * OpenLevel로 레벨이 바뀌면 캐릭터·컴포넌트는 전부 새로 만들어지지만 GameInstance는 게임 내내 살아 있다.
 * 스킬 배치(Q~F 슬롯)·스킬 레벨·남은 스킬 포인트 — USkillManagerComponent가 EndPlay에서 저장하고 BeginPlay에서 복원한다.
 * 사망·클리어 후 정비소 복귀 표시, 레벨 도착 페이드 인 표시 — ALoAPlayerController가 쓰고 읽는다.
 * 게임을 껐다 켜도 남아야 하는 것(스킬 배치·레벨·포인트, 최고 클리어 기록)은 ULoASaveGame으로 디스크에도 쓴다.
 */
UCLASS()
class LOA_API ULoAGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	// 한 번이라도 저장됐는지 — 저장 전(첫 실행)에는 BP 기본 배치를 그대로 쓴다
	UPROPERTY(Transient)
	bool bHasSkillLoadout = false;

	// 슬롯 0~7(Q~F)에 배정된 스킬 행 이름, 빈 슬롯은 NAME_None
	UPROPERTY(Transient)
	TArray<FName> SkillSlotRowNames;

	UPROPERTY(Transient)
	TMap<FName, int32> SkillLevels;

	UPROPERTY(Transient)
	int32 AvailableSkillPoints = 0;

	// 보스 맵에서 사망·클리어해 대기 지역으로 돌아가는 중 — 대기 지역에서 캐릭터가 빙의되면 PlayerStart 대신 정비소 위치로 옮기고 끈다
	UPROPERTY(Transient)
	bool bReturnToRepairStation = false;

	// 페이드 아웃하며 레벨을 옮겼다 — 도착한 레벨에서 검은 화면부터 페이드 인하고 끈다
	UPROPERTY(Transient)
	bool bFadeInOnArrive = false;

	// 최고 클리어 기록 (초, 0 = 없음) / 클리어 횟수
	UPROPERTY(Transient)
	float BestClearTime = 0.f;

	UPROPERTY(Transient)
	int32 ClearCount = 0;

	// 클리어 기록 — 최고 기록을 갱신했으면 true. 디스크에 바로 저장
	bool RecordClear(float ClearSeconds);

	// 스킬 배치·레벨·포인트·클리어 기록을 디스크(SaveSlotName)에 쓴다
	void SaveToDisk();

	static const FString SaveSlotName;

private:
	void LoadFromDisk();
};
