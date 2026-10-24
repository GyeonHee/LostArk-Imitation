#pragma once

#include "CoreMinimal.h"
#include "Stat/StatTypes.h"

struct FEquipmentInstance;
class UEquipmentComponent;

/**
 * 아크 패시브 (로아 T4) — 진화 / 깨달음 / 도약. 노드 정의는 C++ 표(ArkPassiveRegistry.cpp)
 * 수치 출처: Maxroll 플래너 게임 데이터 stats.json의 arkPassives (진화 = 공통 30개, 깨달음·도약 = 소서리스 205번 노드)
 * 한국어 이름: 나무위키 소서리스 운용 문서 + 로아 게임 내 표기
 *
 * 포인트 (공식 가이드 "아크 패시브 포인트 획득"):
 *   진화 = 방어구 부위당 +20(T4 유물) + 전투 레벨 51부터 레벨당 +1
 *   깨달음 = 장신구(등급·부위·연마 단계 표 — 유물 연마 0단계 목걸이 4 / 귀걸이·반지 3) + 전투 레벨 레벨당 +1 + 깨달음 물약 + 카르마 랭크당 +1
 *   도약 = 팔찌(T4 유물 +9) + 전투 레벨 레벨당 +2 + 카르마 랭크당 +2
 *   이 게임엔 전투 레벨 성장이 없어 70 고정, 팔찌·카르마·물약은 아직 없어 0
 * 규칙 (공식 가이드 "아크 패시브"):
 *   진화 — 티어마다 사용 포인트 상한(40/30/20/20/30). 아래 티어를 상한까지 채워야 다음 티어가 열린다
 *   깨달음 — 티어 제한 없음. 연계 노드는 선행 노드가 최고 레벨이어야 하고, 점화/환류처럼 하나만 고를 수 있는 노드가 있다
 *   도약 — 티어 제한 없음. 1티어에 40포인트를 써야 2티어가 열리고, 2티어는 하나만 고를 수 있다
 */
enum class EArkPassiveTree : uint8
{
	Evolution,
	Enlightenment,
	Leap,
	MAX,
};

/** 지금 장비로 쓸 수 있는 포인트 — 트리별 합계와 화면에 보일 내역 줄 */
struct FArkPassiveBudget
{
	int32 Points[3] = { 0, 0, 0 };
	TArray<FString> Lines[3];
	int32 Get(EArkPassiveTree Tree) const { return Points[FMath::Clamp(static_cast<int32>(Tree), 0, 2)]; }
};

struct FArkPassiveNode
{
	FName Id;
	FText Name;
	EArkPassiveTree Tree = EArkPassiveTree::Evolution;
	int32 Tier = 0;          // 0부터 (화면엔 1티어부터)
	int32 Column = 0;        // 화면 열
	int32 Points = 1;        // 레벨 1당 포인트
	int32 MaxLevel = 1;
	FName Requires;          // 선행 노드 (이 노드가 RequiresLevel 이상이어야 배울 수 있다)
	int32 RequiresLevel = 0;
	FName ExclusiveGroup;    // 같은 그룹은 하나만
	TArray<float> Values[4]; // 레벨별 수치 (인덱스 = 레벨 - 1) — 설명의 {0}~{3}
	FString DescTemplate;
	bool bHasEffect = true;  // 이 게임에 시스템이 없어 효과가 없는 노드(각성기·아이덴티티 등)
	FString Note;            // 효과가 없거나 일부만 있는 이유
	TCHAR Glyph = TEXT('?'); // 아이콘 대신 원 안에 쓰는 글자
};

namespace LoAArkPassive
{
	const TArray<FArkPassiveNode>& GetAll();
	const FArkPassiveNode* Find(FName Id);

	FText GetTreeName(EArkPassiveTree Tree);
	FLinearColor GetTreeColor(EArkPassiveTree Tree);

	/** 그 트리의 로아 최대 포인트 (진화 140 / 깨달음 101 / 도약 70) — 표시용. 실제로 쓸 수 있는 건 ComputeBudget */
	int32 GetTotalPoints(EArkPassiveTree Tree);

	// 포인트 규칙 (로아)
	constexpr int32 CombatLevel = 70;              // 전투 레벨 성장이 없어 고정
	constexpr int32 ArmorEvolutionPoints = 20;     // T4 유물 방어구 부위당 (고대 24 — 고대 방어구는 아직 없음)
	/**
	 * 장신구 하나의 깨달음 포인트 — 등급·부위·연마 단계 표 (품질 67 이상 기준, gopenguin.tistory.com/286)
	 *            연마 0  1  2  3
	 *   유물 목걸이   4  5  7  10 / 귀걸이·반지 3  4  6  9
	 *   고대 목걸이   4  6  9  13 / 귀걸이·반지 3  5  7  10
	 * 지금 장신구는 전부 유물(고대 장신구 없음), 연마 시스템이 없어 연마 0단계 → 목걸이 4 + 귀걸이·반지 3×4 = 16
	 */
	int32 GetAccessoryEnlightenment(const FEquipmentInstance& Instance, bool bAncient = false);

	/** 장착 장비에서 트리별 포인트 계산 */
	FArkPassiveBudget ComputeBudget(const UEquipmentComponent* Equipment);
	int32 GetTierCount(EArkPassiveTree Tree);
	/** 그 티어에 쓸 수 있는 포인트 상한 (0 = 제한 없음, 진화만 있음) */
	int32 GetTierCap(EArkPassiveTree Tree, int32 Tier);

	int32 GetLevel(const TMap<FName, int32>& Levels, FName Id);
	/** Tier < 0이면 트리 전체 */
	int32 GetSpent(const TMap<FName, int32>& Levels, EArkPassiveTree Tree, int32 Tier = -1);
	bool IsTierUnlocked(const TMap<FName, int32>& Levels, EArkPassiveTree Tree, int32 Tier);

	/** 배치 전체가 규칙에 맞는지. Budget이 null이면 포인트 합계는 안 본다(장비를 벗어 포인트가 줄어도 배치는 남겨 두려고) */
	bool Validate(const TMap<FName, int32>& Levels, const FArkPassiveBudget* Budget, FText& OutReason);

	/** Delta만큼 한 단계씩 바꾼다(Shift = ±10). 한 단계도 못 바꾸면 false + 이유. 일부만 되면 true */
	bool TryChange(TMap<FName, int32>& Levels, const FArkPassiveBudget& Budget, FName Id, int32 Delta, FText& OutReason);

	float GetValue(const FArkPassiveNode& Node, int32 Index, int32 Level);
	/** 그 레벨의 설명 (Level 0이면 1레벨 기준) */
	FString Describe(FName Id, int32 Level);

	/** 레벨에서 상시 효과 — UStatComponent 수정치 + 스킬별 피해(마나 순환) */
	void BuildStaticModifiers(const TMap<FName, int32>& Levels, TArray<FStatModifier>& OutModifiers, TMap<FName, float>& OutSkillDamage);

	/** 추천 세팅 = 시작 배치 (점화 소서리스) — 우선순위 목록을 포인트가 허락하는 만큼 앞에서부터 채운다 */
	TMap<FName, int32> GetRecommended(const FArkPassiveBudget& Budget);

	// 발동형 노드 수치 (UArkPassiveComponent가 쓴다)
	constexpr float MasterDuration = 10.f;          // 달인
	constexpr int32 MasterMaxStacks = 5;
	constexpr float MasterCritPerStack = 0.014f;
	constexpr float MasterDamagePerStack = 0.017f;
	constexpr float MagickChargeDuration = 45.f;    // 마력 충전
	constexpr float AccelDuration = 45.f;           // 응집되는 마력
	constexpr int32 AccelMaxStacks = 2;
	constexpr float RefluxMPRestore = 0.07f;        // 환류의 기운 — 점멸 시 최대 마나 7%
}
