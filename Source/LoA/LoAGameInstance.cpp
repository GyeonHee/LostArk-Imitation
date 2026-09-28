#include "LoAGameInstance.h"
#include "LoASaveGame.h"
#include "Kismet/GameplayStatics.h"

const FString ULoAGameInstance::SaveSlotName = TEXT("LoA");

void ULoAGameInstance::Init()
{
	Super::Init();
	LoadFromDisk();
}

void ULoAGameInstance::LoadFromDisk()
{
	if (!UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
	{
		UE_LOG(LogTemp, Log, TEXT("[Save] 저장 파일 없음 — BP 기본 스킬 배치로 시작"));
		return;
	}

	const ULoASaveGame* Save = Cast<ULoASaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));
	if (!Save)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Save] 저장 파일을 읽지 못함 — 무시하고 기본값으로 시작"));
		return;
	}

	bHasSkillLoadout = Save->bHasSkillLoadout;
	SkillSlotRowNames = Save->SkillSlotRowNames;
	SkillLevels = Save->SkillLevels;
	AvailableSkillPoints = Save->AvailableSkillPoints;
	BestClearTime = Save->BestClearTime;
	ClearCount = Save->ClearCount;
	UE_LOG(LogTemp, Log, TEXT("[Save] 불러옴 — 스킬 배치 %s, 최고 기록 %.1f초, 클리어 %d회"),
		bHasSkillLoadout ? TEXT("있음") : TEXT("없음"), BestClearTime, ClearCount);
}

void ULoAGameInstance::SaveToDisk()
{
	ULoASaveGame* Save = Cast<ULoASaveGame>(UGameplayStatics::CreateSaveGameObject(ULoASaveGame::StaticClass()));
	if (!Save) return;

	Save->bHasSkillLoadout = bHasSkillLoadout;
	Save->SkillSlotRowNames = SkillSlotRowNames;
	Save->SkillLevels = SkillLevels;
	Save->AvailableSkillPoints = AvailableSkillPoints;
	Save->BestClearTime = BestClearTime;
	Save->ClearCount = ClearCount;

	if (!UGameplayStatics::SaveGameToSlot(Save, SaveSlotName, 0))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Save] 디스크 저장 실패"));
	}
}

bool ULoAGameInstance::RecordClear(float ClearSeconds)
{
	++ClearCount;
	const bool bNewRecord = ClearSeconds > 0.f && (BestClearTime <= 0.f || ClearSeconds < BestClearTime);
	if (bNewRecord)
	{
		BestClearTime = ClearSeconds;
	}
	SaveToDisk();
	return bNewRecord;
}
