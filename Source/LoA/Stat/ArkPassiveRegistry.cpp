#include "Stat/ArkPassiveTypes.h"
#include "Item/ItemTypes.h"
#include "Item/EquipmentComponent.h"
#include "Item/BraceletStone.h"

// ─── 아크 패시브 노드 표 ────────────────────────────────────────────────────
// 수치 = Maxroll stats.json arkPassives (진화 1010100~1040600, 소서리스 깨달음 2370000~2371100, 도약 2375000~2375900)

namespace LoAArkPassive
{
	namespace
	{
		constexpr int32 Ark_TotalPoints[] = { 140, 101, 70 };       // 로아 최대치 — 진화 = 고대 방어구 120 + 전투 레벨 20 (표시용)
		constexpr int32 Ark_EvolutionTierCaps[] = { 40, 30, 20, 20, 30 };
		constexpr int32 Ark_LeapTier2Requirement = 40;              // 도약 — 1티어에 40포인트를 써야 2티어

		using T = EStatModifierTarget;

		TArray<float> Ark_Steps(float Step, int32 Count)
		{
			TArray<float> Out;
			for (int32 i = 1; i <= Count; ++i) Out.Add(Step * i);
			return Out;
		}

		struct FArk_Builder
		{
			TArray<FArkPassiveNode>& Out;
			FArkPassiveNode& Add(const TCHAR* Id, const TCHAR* Name, EArkPassiveTree Tree, int32 Tier, int32 Column, int32 Points, int32 MaxLevel,
				const TCHAR* Desc, TArray<float> V0, TArray<float> V1 = {}, TArray<float> V2 = {}, TArray<float> V3 = {})
			{
				FArkPassiveNode& N = Out.AddDefaulted_GetRef();
				N.Id = Id;
				N.Name = FText::FromString(FString(Name));
				N.Tree = Tree;
				N.Tier = Tier;
				N.Column = Column;
				N.Points = Points;
				N.MaxLevel = MaxLevel;
				N.DescTemplate = Desc;
				N.Values[0] = MoveTemp(V0);
				N.Values[1] = MoveTemp(V1);
				N.Values[2] = MoveTemp(V2);
				N.Values[3] = MoveTemp(V3);
				N.Glyph = Name[0];
				return N;
			}
		};

		TArray<FArkPassiveNode> Ark_Build()
		{
			TArray<FArkPassiveNode> Nodes;
			FArk_Builder B{ Nodes };
			const EArkPassiveTree Evo = EArkPassiveTree::Evolution;
			const EArkPassiveTree Enl = EArkPassiveTree::Enlightenment;
			const EArkPassiveTree Leap = EArkPassiveTree::Leap;
			const TCHAR* NoAwakening = TEXT("각성기·초각성기가 아직 없어 효과 없음");

			// ── 진화 1티어 — 전투 특성 (1P, 레벨당 +50) ──
			B.Add(TEXT("Evo_Crit"),  TEXT("치명"), Evo, 0, 0, 1, 30, TEXT("치명이 {0} 증가한다."), Ark_Steps(50.f, 30));
			B.Add(TEXT("Evo_Spec"),  TEXT("특화"), Evo, 0, 1, 1, 30, TEXT("특화가 {0} 증가한다."), Ark_Steps(50.f, 30));
			B.Add(TEXT("Evo_Dom"),   TEXT("제압"), Evo, 0, 2, 1, 30, TEXT("제압이 {0} 증가한다."), Ark_Steps(50.f, 30));
			B.Add(TEXT("Evo_Swift"), TEXT("신속"), Evo, 0, 3, 1, 30, TEXT("신속이 {0} 증가한다."), Ark_Steps(50.f, 30));
			B.Add(TEXT("Evo_End"),   TEXT("인내"), Evo, 0, 4, 1, 30, TEXT("인내가 {0} 증가한다."), Ark_Steps(50.f, 30));
			{
				FArkPassiveNode& N = B.Add(TEXT("Evo_Exp"), TEXT("숙련"), Evo, 0, 5, 1, 30, TEXT("숙련이 {0} 증가한다."), Ark_Steps(50.f, 30));
				N.bHasEffect = false;
				N.Note = TEXT("숙련을 쓰는 곳(상태이상·무력화 게이지)이 아직 없어 효과 없음");
			}

			// ── 진화 2티어 (10P) ──
			B.Add(TEXT("Evo_BoundlessMP"), TEXT("끝없는 마나"), Evo, 1, 0, 10, 2,
				TEXT("마나를 소모하는 스킬의 재사용 대기시간이 {0}% 감소하고, 마나 소모량이 {1}% 감소한다."), { 7, 14 }, { 10, 20 });
			B.Add(TEXT("Evo_IllicitSpell"), TEXT("금단의 주문"), Evo, 1, 1, 10, 2,
				TEXT("진화형 피해가 {0}% 증가한다. 마나를 소모하는 스킬의 진화형 피해가 {1}% 추가로 증가하고, 마나 소모량이 {2}% 감소한다."), { 5, 10 }, { 5, 10 }, { 6, 12 });
			B.Add(TEXT("Evo_KeenSense"), TEXT("예리한 감각"), Evo, 1, 2, 10, 2,
				TEXT("치명타 적중률이 {0}%, 진화형 피해가 {1}% 증가한다."), { 4, 8 }, { 5, 10 });
			B.Add(TEXT("Evo_LimitBreak"), TEXT("한계 돌파"), Evo, 1, 3, 10, 3,
				TEXT("진화형 피해가 {0}% 증가한다."), { 10, 20, 30 });
			B.Add(TEXT("Evo_OptTraining"), TEXT("최적화 훈련"), Evo, 1, 4, 10, 2,
				TEXT("스킬 재사용 대기시간이 {0}% 감소하고(이동기·기상기 제외), 진화형 피해가 {1}% 증가한다."), { 4, 8 }, { 5, 10 });
			{
				FArkPassiveNode& N = B.Add(TEXT("Evo_Goddess"), TEXT("축복의 여신"), Evo, 1, 5, 10, 3,
					TEXT("전투 중 주변 파티원에게 전투 축복을 건다 — 공격 및 이동 속도가 {0}% 증가한다."), { 3, 6, 9 });
				N.Note = TEXT("파티가 없어 자신에게만 적용");
			}

			// ── 진화 3티어 (10P) ──
			B.Add(TEXT("Evo_UnlimitedMagick"), TEXT("무한한 마력"), Evo, 2, 0, 10, 2,
				TEXT("진화형 피해가 {0}% 증가한다. 마나를 소모하는 스킬의 재사용 대기시간이 {1}%, 마나 소모량이 {2}% 감소한다."), { 8, 16 }, { 7, 14 }, { 8, 16 });
			B.Add(TEXT("Evo_ZealousSmite"), TEXT("혼신의 강타"), Evo, 2, 1, 10, 2,
				TEXT("치명타 적중률이 {0}%, 진화형 피해가 {1}% 증가한다."), { 12, 24 }, { 2, 4 });
			{
				FArkPassiveNode& N = B.Add(TEXT("Evo_Strike"), TEXT("일격"), Evo, 2, 2, 10, 2,
					TEXT("치명타 적중률이 {0}% 증가한다. 방향성 공격 스킬의 치명타 피해가 {1}% 증가한다."), { 10, 20 }, { 16, 32 });
				N.Note = TEXT("소서리스는 방향성(백·헤드) 스킬이 없어 치명타 적중률만");
			}
			B.Add(TEXT("Evo_DestructionCharger"), TEXT("파괴 전차"), Evo, 2, 3, 10, 2,
				TEXT("진화형 피해가 {0}%, 공격 속도가 {1}% 증가한다."), { 12, 24 }, { 4, 8 });
			B.Add(TEXT("Evo_DominionTiming"), TEXT("타이밍 지배"), Evo, 2, 4, 10, 2,
				TEXT("스킬 재사용 대기시간이 {0}% 감소하고(이동기·기상기 제외), 진화형 피해가 {1}% 증가한다."), { 5, 10 }, { 8, 16 });
			{
				FArkPassiveNode& N = B.Add(TEXT("Evo_PassionateDance"), TEXT("정열의 춤사위"), Evo, 2, 5, 10, 2,
					TEXT("공격 적중 시 아이덴티티 게이지 획득량이 {0}% 증가한다. 전투 중 주변 파티원에게 정열의 춤사위를 건다 — 진화형 피해가 {1}% 증가한다."), { 10, 20 }, { 7, 14 });
				N.Requires = TEXT("Evo_Goddess");
				N.RequiresLevel = 3;
				N.Note = TEXT("파티가 없어 진화형 피해는 자신에게만");
			}

			// ── 진화 4티어 (10P, 1레벨) ──
			B.Add(TEXT("Evo_Critical"), TEXT("회심"), Evo, 3, 0, 10, 1,
				TEXT("치명타로 적중 시 피해가 {0}% 증가한다. 받는 피해가 {1}% 감소한다."), { 12 }, { 4 });
			B.Add(TEXT("Evo_Master"), TEXT("달인"), Evo, 3, 1, 10, 1,
				TEXT("받는 피해가 {0}% 감소한다. 이동기·기상기를 제외한 스킬 사용 시 10초간 달인 효과 — 치명타 적중률이 {1}%, 추가 피해가 {2}% 증가한다 (최대 5중첩)."), { 4 }, { 1.4f }, { 1.7f });
			B.Add(TEXT("Evo_Pulverize"), TEXT("분쇄"), Evo, 3, 2, 10, 1,
				TEXT("진화형 피해가 {0}% 증가한다. 받는 피해가 {1}% 감소한다."), { 20 }, { 4 });
			const TCHAR* SupportNote = TEXT("서포터 효과(아군 공격력 강화 등)는 없어 최대 생명력만");
			B.Add(TEXT("Evo_Luminary"), TEXT("선각자"), Evo, 3, 3, 10, 1,
				TEXT("최대 생명력이 {0}% 증가한다. 스킬 사용 시 통찰 효과 — 아군 공격력 강화 효과가 증가한다."), { 6 }).Note = SupportNote;
			B.Add(TEXT("Evo_Advance"), TEXT("진군"), Evo, 3, 4, 10, 1,
				TEXT("최대 생명력이 {0}% 증가한다. 아군에게 보호 효과를 주면 진화된 에테르를 생성한다."), { 6 }).Note = SupportNote;
			B.Add(TEXT("Evo_Prayer"), TEXT("기원"), Evo, 3, 5, 10, 1,
				TEXT("최대 생명력이 {0}% 증가한다. 아군 공격력 강화 효과가 22%, 낙인력이 4% 증가한다."), { 6 }).Note = SupportNote;

			// ── 진화 5티어 (15P) ──
			B.Add(TEXT("Evo_BluntThorn"), TEXT("뭉툭한 가시"), Evo, 4, 0, 15, 2,
				TEXT("진화형 피해가 {0}% 증가한다. 치명타 적중률이 최대 80%로 제한되고, 초과한 치명타 적중률의 {1}%만큼 진화형 피해가 증가한다. 이 노드로 얻는 진화형 피해는 최대 {2}%."),
				{ 7.5f, 15 }, { 125, 150 }, { 52.5f, 75 });
			B.Add(TEXT("Evo_Supersonic"), TEXT("음속 돌파"), Evo, 4, 1, 15, 2,
				TEXT("공격 속도와 이동 속도 증가량의 {0}%만큼 진화형 피해가 증가한다. 둘 다 상한(40%)이면 진화형 피해가 {1}% 추가로 증가하고, 상한을 넘은 증가량의 {3}%만큼 증가한다. 이 노드의 진화형 피해는 최대 {2}%."),
				{ 5, 10 }, { 4, 8 }, { 12, 24 }, { 15, 30 });
			B.Add(TEXT("Evo_Infighting"), TEXT("인파이팅"), Evo, 4, 2, 15, 2,
				TEXT("적중 시 10초간 정면 승부 — 진화형 피해가 {0}% 증가한다 (재사용 대기시간 5초)."), { 9, 18 }).Note = TEXT("레이드 중엔 계속 유지되므로 상시 적용");
			B.Add(TEXT("Evo_StandingStriker"), TEXT("입식 타격가"), Evo, 4, 3, 15, 2,
				TEXT("진화형 피해가 {0}% 증가한다. 전투 시작 시 입식 타격 6중첩 — 중첩당 진화형 피해 {1}% (밀려나면 3중첩 감소, 2초마다 1중첩 회복)."),
				{ 6, 12 }, { 0.75f, 1.5f }).Note = TEXT("중첩은 항상 6으로 계산 (밀려날 때 감소는 아직 없음). 낙인력 효과 없음");
			B.Add(TEXT("Evo_MPFurnace"), TEXT("마나 용광로"), Evo, 4, 4, 15, 2,
				TEXT("마나를 소모하는 스킬이 최대 마나의 2%를 추가로 소모한다. 그 스킬의 기본 마나 소모량 10당 진화형 피해가 {0}% 증가한다 (최대 {1}%)."),
				{ 0.25f, 0.5f }, { 12, 24 }).Note = TEXT("낙인력 효과는 없음");
			{
				FArkPassiveNode& N = B.Add(TEXT("Evo_StableManager"), TEXT("안정된 관리자"), Evo, 4, 5, 15, 2,
					TEXT("낙인력이 {0}% 증가한다. 아이덴티티 게이지 획득량이 {1}% 감소한다."), { 10, 20 }, { 3, 6 });
				N.Note = TEXT("낙인력이 없어 아이덴티티 게이지 감소만");
			}

			// ── 깨달음 (소서리스) — 점화(왼쪽 줄) / 환류(오른쪽 줄) ──
			{
				FArkPassiveNode& N = B.Add(TEXT("Enl_Igniter"), TEXT("점화"), Enl, 0, 1, 8, 3,
					TEXT("마력 해방 발동 시 각성기·이동기·기상기를 제외한 스킬의 남은 재사용 대기시간이 {0}% 감소한다."), { 33, 41, 50 });
				N.ExclusiveGroup = TEXT("IgniteReflux");
			}
			{
				FArkPassiveNode& N = B.Add(TEXT("Enl_Reflux"), TEXT("환류"), Enl, 0, 2, 8, 3,
					TEXT("마력 해방을 사용할 수 없는 대신 스킬 재사용 대기시간이 {0}% 감소한다 (각성기·이동기 제외)."), { 3, 6, 10 });
				N.ExclusiveGroup = TEXT("IgniteReflux");
				N.Note = TEXT("마력 방출(Z)을 쓸 수 없고 신비한 마력은 점멸(X)에만");
			}
			{
				FArkPassiveNode& N = B.Add(TEXT("Enl_IgniterEmber"), TEXT("점화의 불씨"), Enl, 1, 1, 8, 3,
					TEXT("마력 해방 중 치명타 적중률이 {0}% 증가한다."), { 10, 20, 30 });
				N.Requires = TEXT("Enl_Igniter");
				N.RequiresLevel = 3;
			}
			{
				FArkPassiveNode& N = B.Add(TEXT("Enl_RefluxEnergy"), TEXT("환류의 기운"), Enl, 1, 2, 8, 3,
					TEXT("점멸 사용 시 마나를 7% 회복하고, 신비한 마력 소모량이 {0}% 감소한다."), { 3, 6, 10 });
				N.Requires = TEXT("Enl_Reflux");
				N.RequiresLevel = 3;
				N.Note = TEXT("점멸 = X");
			}
			B.Add(TEXT("Enl_FirepowerCharge"), TEXT("화력 충전"), Enl, 2, 0, 2, 5,
				TEXT("기본 조작이 홀딩·캐스팅인 스킬의 피해가 {0}% 증가한다."), { 1.2f, 2.4f, 3.6f, 4.8f, 6 });
			{
				FArkPassiveNode& N = B.Add(TEXT("Enl_Ignite"), TEXT("발화"), Enl, 2, 1, 8, 3,
					TEXT("마력 해방 중 무력화 피해가 20%, 치명타 피해가 {0}% 증가한다."), { 18, 36, 55 });
				N.Requires = TEXT("Enl_IgniterEmber");
				N.RequiresLevel = 3;
				N.Note = TEXT("무력화 피해는 없음 (치명타 피해만)");
			}
			{
				FArkPassiveNode& N = B.Add(TEXT("Enl_RefluxEnhance"), TEXT("환류 강화"), Enl, 2, 2, 8, 3,
					TEXT("각성기를 제외한 모든 스킬의 피해가 {0}% 증가한다."), { 24, 35, 47 });
				N.Requires = TEXT("Enl_RefluxEnergy");
				N.RequiresLevel = 3;
			}
			B.Add(TEXT("Enl_ReleaseSeal"), TEXT("해방 봉인"), Enl, 2, 3, 2, 5,
				TEXT("적에게 주는 피해가 {0}% 증가하지만, 마력 강화·마력 해방 중엔 적에게 주는 피해가 {1}% 감소한다."),
				{ 1.2f, 2.4f, 3.6f, 4.8f, 6 }, { 1, 2, 3, 4, 5 });
			B.Add(TEXT("Enl_BlinkExplosion"), TEXT("점멸 폭발"), Enl, 3, 0, 2, 5,
				TEXT("점멸 사용 시 신비한 마력을 소모하지 않는다. 점멸 재사용 대기시간이 20초 증가한다. 마력 강화·마력 해방 중 피해가 {0}% 증가한다."), { 1, 2, 3, 4, 5 });
			{
				FArkPassiveNode& N = B.Add(TEXT("Enl_MagickCycle"), TEXT("마나 순환"), Enl, 3, 1, 8, 3,
					TEXT("종말의 날·익스플로전·천벌의 피해가 {0}% 증가한다. 마력 해방 사용 시 적에게 주는 피해가 {1}% 증가하고, 끝나면 공격 속도가 8% 증가한다(최대 30초)."),
					{ 5, 10, 15 }, { 10, 14, 18 });
				N.Requires = TEXT("Enl_Ignite");
				N.RequiresLevel = 3;
			}
			{
				FArkPassiveNode& N = B.Add(TEXT("Enl_MagickCharge"), TEXT("마력 충전"), Enl, 3, 2, 8, 3,
					TEXT("점멸 사용 시 마력 충전 — 45초간 공격력이 {0}% 증가한다."), { 8, 17, 26 });
				N.Requires = TEXT("Enl_RefluxEnhance");
				N.RequiresLevel = 3;
				N.Note = TEXT("점멸 = X");
			}
			B.Add(TEXT("Enl_ConcentratedMagick"), TEXT("응집되는 마력"), Enl, 3, 3, 2, 5,
				TEXT("마력 해방을 사용할 수 없는 대신, 점멸 사용 시 45초간 마력 가속 — 적에게 주는 피해가 {0}% 증가한다 (최대 2중첩). 최대 중첩 시 공격 속도가 {1}% 증가한다."),
				{ 0.6f, 1.2f, 1.8f, 2.4f, 3 }, { 3, 3.5f, 4, 4.5f, 5 }).Note = TEXT("마력 방출(Z)을 쓸 수 없음. 점멸 = X");

			// ── 도약 (소서리스) — 전부 각성기·초각성기 관련 ──
			B.Add(TEXT("Leap_Transcendent"), TEXT("초월적인 힘"), Leap, 0, 0, 4, 5, TEXT("초각성기 피해가 {0}% 증가한다."), Ark_Steps(10.f, 5));
			B.Add(TEXT("Leap_ChargedFury"), TEXT("충전된 힘"), Leap, 0, 1, 4, 5, TEXT("초각성기 게이지가 가득 차면 각성기의 남은 재사용 대기시간이 {0}% 감소한다."), Ark_Steps(10.f, 5));
			B.Add(TEXT("Leap_AwakeningAmp"), TEXT("각성 증폭기"), Leap, 0, 2, 2, 3, TEXT("각성기 사용 가능 횟수가 {0}회 증가한다."), { 1, 2, 3 });
			B.Add(TEXT("Leap_UnleashedPower"), TEXT("풀려난 힘"), Leap, 0, 3, 4, 5, TEXT("초각성 스킬 피해가 {0}% 증가한다."), Ark_Steps(3.f, 5));
			B.Add(TEXT("Leap_ReleasePotential"), TEXT("잠재력 해방"), Leap, 0, 4, 4, 5, TEXT("초각성 스킬 재사용 대기시간이 {0}% 감소한다."), Ark_Steps(2.f, 5));
			B.Add(TEXT("Leap_InstantSpell"), TEXT("즉각적인 주문"), Leap, 0, 5, 2, 3, TEXT("초각성 스킬 캐스팅 속도가 {0}% 증가하고 마나 소모량이 {1}% 감소한다."), { 4, 8, 12 }, { 30, 60, 90 });
			B.Add(TEXT("Leap_GreatExplosion"), TEXT("대폭발"), Leap, 1, 0, 10, 3,
				TEXT("라바 블래스트가 적에게 주는 피해가 {0}% 증가한다. 추가 폭발이 1회로 줄고 그 피해가 {1}% 증가한다."), { 5, 10, 15 }, { 95, 190, 285 });
			B.Add(TEXT("Leap_Outburst"), TEXT("분출"), Leap, 1, 1, 10, 3,
				TEXT("라바 블래스트가 적에게 주는 피해가 {0}% 감소한다. 마력 해방 발동 시 라바 블래스트의 재사용 대기시간이 즉시 초기화된다."), { 38, 25, 10 });
			B.Add(TEXT("Leap_PiercingGlacier"), TEXT("꿰뚫는 빙하"), Leap, 1, 2, 10, 3,
				TEXT("러닝 글레이셔가 관통 공격으로 바뀐다. 얼음 범위가 5m 줄지만 최대 4번 충돌 피해를 주고, 마지막 폭발 피해가 {0}% 증가한다."), { 10, 30, 50 });
			B.Add(TEXT("Leap_FrostMastery"), TEXT("냉기 숙련"), Leap, 1, 3, 10, 3,
				TEXT("러닝 글레이셔가 적에게 주는 피해가 {0}%, 캐스팅 속도가 {1}% 증가한다."), { 18, 36, 54 }, { 4, 8, 12 });
			for (FArkPassiveNode& N : Nodes)
			{
				if (N.Tree != Leap) continue;
				N.bHasEffect = false;
				N.Note = NoAwakening;
				if (N.Tier == 1) N.ExclusiveGroup = TEXT("LeapTier2");
			}
			return Nodes;
		}

		int32 Ark_TreeIndex(EArkPassiveTree Tree) { return FMath::Clamp(static_cast<int32>(Tree), 0, 2); }

		void Ark_Add(TArray<FStatModifier>& Out, T Target, double Value)
		{
			if (Value == 0.0) return;
			FStatModifier& M = Out.AddDefaulted_GetRef();
			M.Target = Target;
			M.Value = Value;
		}
	}

	const TArray<FArkPassiveNode>& GetAll()
	{
		static const TArray<FArkPassiveNode> Nodes = Ark_Build();
		return Nodes;
	}

	const FArkPassiveNode* Find(FName Id)
	{
		if (Id.IsNone()) return nullptr;
		return GetAll().FindByPredicate([Id](const FArkPassiveNode& N) { return N.Id == Id; });
	}

	FText GetTreeName(EArkPassiveTree Tree)
	{
		switch (Tree)
		{
		case EArkPassiveTree::Evolution:     return NSLOCTEXT("ArkPassive", "Evolution", "진화");
		case EArkPassiveTree::Enlightenment: return NSLOCTEXT("ArkPassive", "Enlightenment", "깨달음");
		default:                             return NSLOCTEXT("ArkPassive", "Leap", "도약");
		}
	}

	FLinearColor GetTreeColor(EArkPassiveTree Tree)
	{
		switch (Tree)
		{
		case EArkPassiveTree::Evolution:     return FLinearColor(1.f, 0.62f, 0.18f, 1.f);   // 주황
		case EArkPassiveTree::Enlightenment: return FLinearColor(0.30f, 0.62f, 1.f, 1.f);   // 파랑
		default:                             return FLinearColor(0.40f, 0.95f, 0.35f, 1.f); // 초록
		}
	}

	int32 GetTotalPoints(EArkPassiveTree Tree) { return Ark_TotalPoints[Ark_TreeIndex(Tree)]; }

	int32 GetTierCount(EArkPassiveTree Tree)
	{
		int32 Count = 0;
		for (const FArkPassiveNode& N : GetAll())
		{
			if (N.Tree == Tree) Count = FMath::Max(Count, N.Tier + 1);
		}
		return Count;
	}

	int32 GetTierCap(EArkPassiveTree Tree, int32 Tier)
	{
		if (Tree != EArkPassiveTree::Evolution || Tier < 0 || Tier >= UE_ARRAY_COUNT(Ark_EvolutionTierCaps)) return 0;
		return Ark_EvolutionTierCaps[Tier];
	}

	int32 GetLevel(const TMap<FName, int32>& Levels, FName Id)
	{
		const int32* Found = Levels.Find(Id);
		return Found ? *Found : 0;
	}

	int32 GetSpent(const TMap<FName, int32>& Levels, EArkPassiveTree Tree, int32 Tier)
	{
		int32 Spent = 0;
		for (const TPair<FName, int32>& Pair : Levels)
		{
			const FArkPassiveNode* N = Find(Pair.Key);
			if (N && N->Tree == Tree && (Tier < 0 || N->Tier == Tier)) Spent += N->Points * Pair.Value;
		}
		return Spent;
	}

	bool IsTierUnlocked(const TMap<FName, int32>& Levels, EArkPassiveTree Tree, int32 Tier)
	{
		if (Tier <= 0) return true;
		if (Tree == EArkPassiveTree::Evolution)
		{
			for (int32 i = 0; i < Tier; ++i)
			{
				if (GetSpent(Levels, Tree, i) < GetTierCap(Tree, i)) return false;
			}
			return true;
		}
		if (Tree == EArkPassiveTree::Leap)
		{
			return GetSpent(Levels, Tree, 0) >= Ark_LeapTier2Requirement;
		}
		return true;  // 깨달음은 티어 제한 없음 (연계 노드만 선행 조건)
	}

	int32 GetAccessoryEnlightenment(const FEquipmentInstance& Instance, bool bAncient)
	{
		// [등급][목걸이/귀걸이·반지][연마 단계]
		static constexpr int32 Table[2][2][4] = {
			{ { 4, 5, 7, 10 }, { 3, 4, 6, 9 } },   // 유물
			{ { 4, 6, 9, 13 }, { 3, 5, 7, 10 } },  // 고대
		};
		const FItemDefinition* Def = LoAItems::Find(Instance.ItemId);
		const int32 Part = (Def && Def->EquipSlot == EEquipSlot::Necklace) ? 0 : 1;
		return Table[bAncient ? 1 : 0][Part][FMath::Clamp(Instance.PolishLevel, 0, 3)];
	}

	FArkPassiveBudget ComputeBudget(const UEquipmentComponent* Equipment)
	{
		FArkPassiveBudget B;
		const int32 LevelBonus = FMath::Max(0, CombatLevel - 50);

		// 진화 — 방어구 5부위
		int32 ArmorCount = 0;
		for (EEquipSlot Slot : { EEquipSlot::Helmet, EEquipSlot::Shoulder, EEquipSlot::Chest, EEquipSlot::Pants, EEquipSlot::Gloves })
		{
			if (Equipment && Equipment->GetEquipped(Slot)) ++ArmorCount;
		}
		B.Points[0] = ArmorCount * ArmorEvolutionPoints + LevelBonus;
		B.Lines[0].Add(FString::Printf(TEXT("방어구 %d부위 (T4 유물, 부위당 +%d)  +%d"), ArmorCount, ArmorEvolutionPoints, ArmorCount * ArmorEvolutionPoints));
		B.Lines[0].Add(FString::Printf(TEXT("전투 레벨 %d  +%d"), CombatLevel, LevelBonus));

		// 깨달음 — 장신구 5개
		int32 AccessoryPoints = 0;
		for (EEquipSlot Slot : { EEquipSlot::Necklace, EEquipSlot::Earring1, EEquipSlot::Earring2, EEquipSlot::Ring1, EEquipSlot::Ring2 })
		{
			const FEquipmentInstance* Instance = Equipment ? Equipment->GetEquipped(Slot) : nullptr;
			if (!Instance) continue;
			const int32 Points = GetAccessoryEnlightenment(*Instance);
			AccessoryPoints += Points;
			B.Lines[1].Add(FString::Printf(TEXT("%s  +%d"), *StaticEnum<EEquipSlot>()->GetDisplayNameTextByValue(static_cast<int64>(Slot)).ToString(), Points));
		}
		B.Points[1] = AccessoryPoints + LevelBonus;
		B.Lines[1].Add(FString::Printf(TEXT("전투 레벨 %d  +%d"), CombatLevel, LevelBonus));
		B.Lines[1].Add(TEXT("깨달음 물약 · 카르마  +0 (아직 없음)"));

		// 도약 — 팔찌 (T4 유물 +9, 고대 +18 — 고대 팔찌는 아직 없음)
		const int32 BraceletPoints = (Equipment && Equipment->GetEquipped(EEquipSlot::Bracelet)) ? LoABracelet::LeapPointsRelic : 0;
		B.Points[2] = BraceletPoints + LevelBonus * 2;
		B.Lines[2].Add(FString::Printf(TEXT("팔찌 (T4 유물)  +%d"), BraceletPoints));
		B.Lines[2].Add(FString::Printf(TEXT("전투 레벨 %d  +%d"), CombatLevel, LevelBonus * 2));
		B.Lines[2].Add(TEXT("카르마  +0 (아직 없음)"));
		return B;
	}

	bool Validate(const TMap<FName, int32>& Levels, const FArkPassiveBudget* Budget, FText& OutReason)
	{
		TMap<FName, FName> UsedGroups;
		for (const TPair<FName, int32>& Pair : Levels)
		{
			if (Pair.Value <= 0) continue;
			const FArkPassiveNode* N = Find(Pair.Key);
			if (!N) continue;
			if (Pair.Value > N->MaxLevel)
			{
				OutReason = FText::Format(NSLOCTEXT("ArkPassive", "MaxLevel", "{0}은(는) 최고 레벨입니다."), N->Name);
				return false;
			}
			if (!IsTierUnlocked(Levels, N->Tree, N->Tier))
			{
				OutReason = N->Tree == EArkPassiveTree::Leap
					? FText::Format(NSLOCTEXT("ArkPassive", "LeapLocked", "1티어에 도약 포인트를 {0} 이상 사용해야 2티어를 배울 수 있습니다."), FText::AsNumber(Ark_LeapTier2Requirement))
					: NSLOCTEXT("ArkPassive", "TierLocked", "아래 티어의 포인트를 모두 사용해야 이 티어를 배울 수 있습니다.");
				return false;
			}
			if (!N->Requires.IsNone() && GetLevel(Levels, N->Requires) < N->RequiresLevel)
			{
				const FArkPassiveNode* Req = Find(N->Requires);
				OutReason = FText::Format(NSLOCTEXT("ArkPassive", "Requires", "[{0}]을(를) {1}레벨까지 배워야 합니다."),
					Req ? Req->Name : FText::GetEmpty(), FText::AsNumber(N->RequiresLevel));
				return false;
			}
			if (!N->ExclusiveGroup.IsNone())
			{
				if (const FName* Other = UsedGroups.Find(N->ExclusiveGroup))
				{
					const FArkPassiveNode* O = Find(*Other);
					OutReason = FText::Format(NSLOCTEXT("ArkPassive", "Exclusive", "[{0}]을(를) 배워서 이 아크 패시브는 배울 수 없습니다."), O ? O->Name : FText::GetEmpty());
					return false;
				}
				UsedGroups.Add(N->ExclusiveGroup, N->Id);
			}
		}
		for (int32 t = 0; t < static_cast<int32>(EArkPassiveTree::MAX); ++t)
		{
			const EArkPassiveTree Tree = static_cast<EArkPassiveTree>(t);
			if (Budget && GetSpent(Levels, Tree) > Budget->Get(Tree))
			{
				OutReason = FText::Format(NSLOCTEXT("ArkPassive", "NoPoints", "{0} 포인트가 부족합니다."), GetTreeName(Tree));
				return false;
			}
			for (int32 Tier = 0; Tier < GetTierCount(Tree); ++Tier)
			{
				const int32 Cap = GetTierCap(Tree, Tier);
				if (Cap > 0 && GetSpent(Levels, Tree, Tier) > Cap)
				{
					OutReason = FText::Format(NSLOCTEXT("ArkPassive", "TierCap", "{0}티어에는 포인트를 {1}까지만 쓸 수 있습니다."), FText::AsNumber(Tier + 1), FText::AsNumber(Cap));
					return false;
				}
			}
		}
		return true;
	}

	bool TryChange(TMap<FName, int32>& Levels, const FArkPassiveBudget& Budget, FName Id, int32 Delta, FText& OutReason)
	{
		const FArkPassiveNode* N = Find(Id);
		if (!N || Delta == 0) return false;
		const int32 Step = Delta > 0 ? 1 : -1;
		int32 Done = 0;
		for (int32 i = 0; i < FMath::Abs(Delta); ++i)
		{
			const int32 Current = GetLevel(Levels, Id);
			const int32 Next = Current + Step;
			if (Next < 0)
			{
				if (Done == 0) OutReason = NSLOCTEXT("ArkPassive", "AlreadyZero", "배우지 않은 아크 패시브입니다.");
				break;
			}
			if (Next > N->MaxLevel)
			{
				if (Done == 0) OutReason = FText::Format(NSLOCTEXT("ArkPassive", "MaxLevel", "{0}은(는) 최고 레벨입니다."), N->Name);
				break;
			}
			TMap<FName, int32> Trial = Levels;
			if (Next == 0) Trial.Remove(Id);
			else Trial.Add(Id, Next);
			FText Reason;
			// 내리기는 포인트 합계를 안 본다 — 장비를 벗어 포인트가 모자란 상태에서도 줄여 나갈 수 있게
			if (!Validate(Trial, Step > 0 ? &Budget : nullptr, Reason))
			{
				// 내리기가 막히는 건 위 티어·연계 노드가 이 노드에 기대고 있을 때
				if (Done == 0) OutReason = Step < 0 ? NSLOCTEXT("ArkPassive", "DependsOn", "이 아크 패시브에 기대는 상위 아크 패시브를 먼저 내려 주세요.") : Reason;
				break;
			}
			Levels = MoveTemp(Trial);
			++Done;
		}
		return Done > 0;
	}

	float GetValue(const FArkPassiveNode& Node, int32 Index, int32 Level)
	{
		if (Index < 0 || Index >= UE_ARRAY_COUNT(Node.Values) || Node.Values[Index].Num() == 0 || Level <= 0) return 0.f;
		return Node.Values[Index][FMath::Clamp(Level, 1, Node.Values[Index].Num()) - 1];
	}

	FString Describe(FName Id, int32 Level)
	{
		const FArkPassiveNode* N = Find(Id);
		if (!N) return FString();
		const int32 L = FMath::Clamp(Level, 1, N->MaxLevel);
		FString Text = N->DescTemplate;
		for (int32 i = 0; i < UE_ARRAY_COUNT(N->Values); ++i)
		{
			const float V = GetValue(*N, i, L);
			const FString Num = FMath::IsNearlyEqual(V, FMath::RoundToFloat(V)) ? FString::Printf(TEXT("%.0f"), V) : FString::SanitizeFloat(V);
			Text = Text.Replace(*FString::Printf(TEXT("{%d}"), i), *Num);
		}
		return Text;
	}

	void BuildStaticModifiers(const TMap<FName, int32>& Levels, TArray<FStatModifier>& OutModifiers, TMap<FName, float>& OutSkillDamage)
	{
		OutModifiers.Reset();
		OutSkillDamage.Reset();
		for (const TPair<FName, int32>& Pair : Levels)
		{
			const FArkPassiveNode* N = Find(Pair.Key);
			const int32 L = Pair.Value;
			if (!N || L <= 0 || !N->bHasEffect) continue;
			// 표의 % 값 → 비율
			auto P = [N, L](int32 Index) { return GetValue(*N, Index, L) / 100.0; };
			auto V = [N, L](int32 Index) { return static_cast<double>(GetValue(*N, Index, L)); };
			TArray<FStatModifier>& O = OutModifiers;
			const FString S = N->Id.ToString();

			if      (S == TEXT("Evo_Crit"))  Ark_Add(O, T::Crit, V(0));
			else if (S == TEXT("Evo_Spec"))  Ark_Add(O, T::Specialization, V(0));
			else if (S == TEXT("Evo_Dom"))   Ark_Add(O, T::Domination, V(0));
			else if (S == TEXT("Evo_Swift")) Ark_Add(O, T::Swiftness, V(0));
			else if (S == TEXT("Evo_End"))   Ark_Add(O, T::Endurance, V(0));
			else if (S == TEXT("Evo_BoundlessMP"))     { Ark_Add(O, T::ManaSkillCooldown, P(0)); Ark_Add(O, T::ManaCostReduction, P(1)); }
			else if (S == TEXT("Evo_IllicitSpell"))    { Ark_Add(O, T::EvolutionDamage, P(0)); Ark_Add(O, T::ManaSkillEvolutionDamage, P(1)); Ark_Add(O, T::ManaCostReduction, P(2)); }
			else if (S == TEXT("Evo_KeenSense"))       { Ark_Add(O, T::CritRate, P(0)); Ark_Add(O, T::EvolutionDamage, P(1)); }
			else if (S == TEXT("Evo_LimitBreak"))      { Ark_Add(O, T::EvolutionDamage, P(0)); }
			else if (S == TEXT("Evo_OptTraining"))     { Ark_Add(O, T::CooldownReduction, P(0)); Ark_Add(O, T::EvolutionDamage, P(1)); }
			else if (S == TEXT("Evo_Goddess"))         { Ark_Add(O, T::SpeedBonus, P(0)); }
			else if (S == TEXT("Evo_UnlimitedMagick")) { Ark_Add(O, T::EvolutionDamage, P(0)); Ark_Add(O, T::ManaSkillCooldown, P(1)); Ark_Add(O, T::ManaCostReduction, P(2)); }
			else if (S == TEXT("Evo_ZealousSmite"))    { Ark_Add(O, T::CritRate, P(0)); Ark_Add(O, T::EvolutionDamage, P(1)); }
			else if (S == TEXT("Evo_Strike"))          { Ark_Add(O, T::CritRate, P(0)); }
			else if (S == TEXT("Evo_DestructionCharger")) { Ark_Add(O, T::EvolutionDamage, P(0)); Ark_Add(O, T::AttackSpeedBonus, P(1)); }
			else if (S == TEXT("Evo_DominionTiming"))  { Ark_Add(O, T::CooldownReduction, P(0)); Ark_Add(O, T::EvolutionDamage, P(1)); }
			else if (S == TEXT("Evo_PassionateDance")) { Ark_Add(O, T::EvolutionDamage, P(1)); }
			else if (S == TEXT("Evo_Critical"))        { Ark_Add(O, T::CritHitDamage, P(0)); Ark_Add(O, T::IncomingDamage, -P(1)); }
			else if (S == TEXT("Evo_Master"))          { Ark_Add(O, T::IncomingDamage, -P(0)); }  // 중첩은 컴포넌트가 따로
			else if (S == TEXT("Evo_Pulverize"))       { Ark_Add(O, T::EvolutionDamage, P(0)); Ark_Add(O, T::IncomingDamage, -P(1)); }
			else if (S == TEXT("Evo_Luminary") || S == TEXT("Evo_Advance") || S == TEXT("Evo_Prayer")) { Ark_Add(O, T::MaxHPPercent, P(0)); }
			else if (S == TEXT("Evo_BluntThorn"))
			{
				Ark_Add(O, T::EvolutionDamage, P(0));
				Ark_Add(O, T::CritRateCap, 0.8);
				Ark_Add(O, T::CritOverflowToEvolution, P(1));
				Ark_Add(O, T::CritOverflowEvolutionCap, P(2) - P(0));  // 노드 전체 상한 = 기본분 + 전환분
			}
			else if (S == TEXT("Evo_Supersonic"))
			{
				Ark_Add(O, T::SpeedToEvolution, P(0));
				Ark_Add(O, T::SpeedCapEvolutionBonus, P(1));
				Ark_Add(O, T::SpeedToEvolutionCap, P(2));
				Ark_Add(O, T::SpeedOverCapToEvolution, P(3));
			}
			else if (S == TEXT("Evo_Infighting"))      { Ark_Add(O, T::EvolutionDamage, P(0)); }
			else if (S == TEXT("Evo_StandingStriker")) { Ark_Add(O, T::EvolutionDamage, P(0) + P(1) * 6.0); }
			else if (S == TEXT("Evo_MPFurnace"))
			{
				Ark_Add(O, T::ManaCostMaxMPPercent, 0.02);
				Ark_Add(O, T::ManaCostToEvolution, P(0) / 10.0);  // 기본 마나 1당
				Ark_Add(O, T::ManaCostToEvolutionCap, P(1));
			}
			else if (S == TEXT("Enl_Reflux"))             { Ark_Add(O, T::CooldownReduction, P(0)); }
			else if (S == TEXT("Enl_FirepowerCharge"))    { Ark_Add(O, T::CastSkillDamage, P(0)); }
			else if (S == TEXT("Enl_RefluxEnhance"))      { Ark_Add(O, T::MultiplicativeDamage, P(0)); }
			else if (S == TEXT("Enl_ReleaseSeal"))        { Ark_Add(O, T::MultiplicativeDamage, P(0)); }
			else if (S == TEXT("Enl_MagickCycle"))
			{
				for (const TCHAR* Skill : { TEXT("종말의날"), TEXT("익스플로전"), TEXT("천벌") })
				{
					OutSkillDamage.FindOrAdd(Skill) += static_cast<float>(P(0));
				}
			}
			// 환류의 기운·마력 충전·응집되는 마력·달인 중첩은 발동형 — UArkPassiveComponent
			// 점화·점화의 불씨·발화·마나 순환(해방 중)·점멸 폭발·해방 봉인(강화·해방 중)·정열의 춤사위·안정된 관리자(게이지)는 USorceressIdentityComponent
		}
	}

	TMap<FName, int32> GetRecommended(const FArkPassiveBudget& Budget)
	{
		// 점화 소서리스 — 앞에서부터 포인트가 허락하는 만큼 (못 올리면 다음 항목으로)
		static const TPair<const TCHAR*, int32> Order[] = {
			// 진화: 특화·치명 / 예리한 감각·한계 돌파 / 무한한 마력 / 달인·분쇄 / 마나 용광로
			{ TEXT("Evo_Spec"), 30 }, { TEXT("Evo_Crit"), 10 },
			{ TEXT("Evo_KeenSense"), 2 }, { TEXT("Evo_LimitBreak"), 1 },
			{ TEXT("Evo_UnlimitedMagick"), 2 },
			{ TEXT("Evo_Master"), 1 }, { TEXT("Evo_Pulverize"), 1 },
			{ TEXT("Evo_MPFurnace"), 2 },
			// 깨달음: 점화 > 점화의 불씨 > 발화 > 마나 순환, 남는 포인트는 점멸 폭발·화력 충전
			{ TEXT("Enl_Igniter"), 3 }, { TEXT("Enl_IgniterEmber"), 3 }, { TEXT("Enl_Ignite"), 3 }, { TEXT("Enl_MagickCycle"), 3 },
			{ TEXT("Enl_BlinkExplosion"), 1 }, { TEXT("Enl_FirepowerCharge"), 5 }, { TEXT("Enl_BlinkExplosion"), 5 },
			// 도약: 풀려난 힘·잠재력 해방 → 대폭발
			{ TEXT("Leap_UnleashedPower"), 5 }, { TEXT("Leap_ReleasePotential"), 5 }, { TEXT("Leap_GreatExplosion"), 3 },
		};
		TMap<FName, int32> L;
		for (const TPair<const TCHAR*, int32>& Entry : Order)
		{
			const FName Id(Entry.Key);
			while (GetLevel(L, Id) < Entry.Value)
			{
				TMap<FName, int32> Trial = L;
				Trial.Add(Id, GetLevel(L, Id) + 1);
				FText Reason;
				if (!Validate(Trial, &Budget, Reason)) break;
				L = MoveTemp(Trial);
			}
		}
		return L;
	}
}
