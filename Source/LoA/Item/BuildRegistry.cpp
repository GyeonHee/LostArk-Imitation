#include "Item/BuildTypes.h"
#include "Item/ItemTypes.h"
#include "Engine/Texture2D.h"
#include "Item/InventoryComponent.h"

// ─── 보석 ─────────────────────────────────────────────────────────────────

namespace LoAGems
{
	namespace
	{
		// Maxroll Gem System Guide — T4 보석 Lv1~10
		const float Gem_Damage[MaxLevel] = { 0.08f, 0.12f, 0.16f, 0.20f, 0.24f, 0.28f, 0.32f, 0.36f, 0.40f, 0.44f };
		const float Gem_Cooldown[MaxLevel] = { 0.06f, 0.08f, 0.10f, 0.12f, 0.14f, 0.16f, 0.18f, 0.20f, 0.22f, 0.24f };
		const float Gem_AttackPower[MaxLevel] = { 0.f, 0.0005f, 0.001f, 0.002f, 0.003f, 0.0045f, 0.006f, 0.008f, 0.01f, 0.012f };

		const TCHAR* Gem_Prefix(EGemType Type) { return Type == EGemType::Damage ? TEXT("Gem_Dmg_") : TEXT("Gem_Cd_"); }
	}

	FName MakeGemId(EGemType Type, int32 Level)
	{
		return FName(*FString::Printf(TEXT("%s%02d"), Gem_Prefix(Type), FMath::Clamp(Level, 1, MaxLevel)));
	}

	bool ParseGemId(FName ItemId, EGemType& OutType, int32& OutLevel)
	{
		const FString S = ItemId.ToString();
		for (const EGemType Type : { EGemType::Damage, EGemType::Cooldown })
		{
			const FString Prefix = Gem_Prefix(Type);
			if (S.StartsWith(Prefix))
			{
				OutType = Type;
				OutLevel = FCString::Atoi(*S.RightChop(Prefix.Len()));
				return OutLevel >= 1 && OutLevel <= MaxLevel;
			}
		}
		return false;
	}

	float GetEffect(EGemType Type, int32 Level)
	{
		const int32 L = FMath::Clamp(Level, 1, MaxLevel) - 1;
		return Type == EGemType::Damage ? Gem_Damage[L] : Gem_Cooldown[L];
	}

	float GetAttackPowerPercent(int32 Level)
	{
		return Gem_AttackPower[FMath::Clamp(Level, 1, MaxLevel) - 1];
	}

	FText GetGemName(EGemType Type, int32 Level)
	{
		return FText::FromString(FString::Printf(TEXT("%d레벨 %s의 보석"), Level, Type == EGemType::Damage ? TEXT("겁화") : TEXT("작열")));
	}

	FString DescribeGem(EGemType Type, int32 Level)
	{
		const FString Main = Type == EGemType::Damage
			? FString::Printf(TEXT("스킬 피해 +%.2f%%"), GetEffect(Type, Level) * 100.f)
			: FString::Printf(TEXT("스킬 재사용 대기시간 -%.2f%%"), GetEffect(Type, Level) * 100.f);
		const float Atk = GetAttackPowerPercent(Level);
		return Atk > 0.f ? FString::Printf(TEXT("%s\n기본 공격력 +%.2f%%"), *Main, Atk * 100.f) : Main;
	}

	const FName GemBox(TEXT("GemBox_Lv1"));

	bool OpenGemBox(UInventoryComponent* Inventory, int32 SlotIndex, FText& OutFailReason)
	{
		if (!Inventory || !Inventory->IsValidIndex(SlotIndex) || Inventory->GetSlot(SlotIndex).ItemId != GemBox) return false;
		// 겁화·작열을 미리 굴려 둔 뒤 자리 확인 — 칸마다 1개라 10칸 필요
		TMap<FName, int32> Gems;
		for (int32 i = 0; i < BoxGemCount; ++i) Gems.FindOrAdd(MakeGemId(FMath::RandBool() ? EGemType::Damage : EGemType::Cooldown, 1)) += 1;
		if (!Inventory->CanAddItems(Gems))
		{
			OutFailReason = FText::FromString(FString::Printf(TEXT("소지품 빈 칸이 %d개 필요합니다."), BoxGemCount));
			return false;
		}
		Inventory->RemoveFromSlot(SlotIndex, 1);
		for (const TPair<FName, int32>& Pair : Gems) Inventory->AddItem(Pair.Key, Pair.Value);
		return true;
	}

	void RegisterItems(TMap<FName, FItemDefinition>& Map)
	{
		{
			FItemDefinition Box;
			Box.ItemId = GemBox;
			Box.DisplayName = NSLOCTEXT("Item", "GemBox", "1레벨 보석 상자");
			Box.Description = FText::FromString(FString::Printf(TEXT("1레벨 겁화·작열의 보석 %d개 (종류는 랜덤)\n\n우클릭: 열기 (빈 칸 %d개 필요)"), BoxGemCount, BoxGemCount));
			Box.Category = EItemCategory::Gem;
			Box.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LostArk/UI/GemIcon/T_GemBox.T_GemBox")));
			Box.MaxStack = 99;
			Box.SortOrder = -1;  // 보석보다 앞
			Box.SellPrice = 0;
			Map.Add(Box.ItemId, Box);
		}
		for (const EGemType Type : { EGemType::Damage, EGemType::Cooldown })
		{
			for (int32 Level = 1; Level <= MaxLevel; ++Level)
			{
				FItemDefinition Def;
				Def.ItemId = MakeGemId(Type, Level);
				Def.DisplayName = GetGemName(Type, Level);
				Def.Description = FText::FromString(DescribeGem(Type, Level) + TEXT("\n\n캐릭터 정보(P) [보석] 탭을 연 채로 우클릭 → 스킬을 골라 장착"));
				Def.Category = EItemCategory::Gem;
				const FString Icon = FString::Printf(TEXT("T_Gem_%s_%02d"), Type == EGemType::Damage ? TEXT("Dmg") : TEXT("Cd"), Level);
				Def.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(FString::Printf(TEXT("/Game/LostArk/UI/GemIcon/%s.%s"), *Icon, *Icon)));
				Def.MaxStack = 1;  // 로아처럼 칸마다 1개 — 겹치지 않는다
				// 정렬: 겁화 먼저, 높은 레벨 먼저
				Def.SortOrder = (Type == EGemType::Damage ? 0 : 100) + (MaxLevel - Level);
				Def.SellPrice = 100 * Level;
				Map.Add(Def.ItemId, Def);
			}
		}
	}
}

// ─── 각인 ─────────────────────────────────────────────────────────────────

namespace LoAEngravings
{
	const FName Adrenaline(TEXT("Adrenaline"));
	const FName MagickStream(TEXT("MagickStream"));
	const FName EtherPredator(TEXT("EtherPredator"));
	const FName CrushingFist(TEXT("CrushingFist"));
	const FName Propulsion(TEXT("Propulsion"));

	namespace
	{
		/**
		 * 각인 한 줄 — 유물 0단계 / 4단계 값(중간 단계는 등간격, 게임 데이터도 그렇다).
		 * 효과가 둘이면 A·B. bWorks = 이 게임에 그 시스템이 있어 실제로 효과가 있는가 (없으면 장착만 되고 설명에 표시)
		 * 수치 출처: Maxroll 게임 데이터 engraving 1xxx 의 유물 0단계(desc[4])·4단계(desc[8]), 어빌리티 스톤 0
		 */
		struct FEngRow
		{
			const TCHAR* Id;
			const TCHAR* Name;
			float A0, A4;
			float B0, B4;
			bool bWorks;
			const TCHAR* NoEffectReason;
		};

		// 순서 = 각인 효과 창 카드 순서 (로아 각인 창처럼 가나다 대신 데이터 순)
		const FEngRow Eng_Rows[] = {
			{ TEXT("Grudge"),           TEXT("원한"),          0.18f,  0.21f,  0.20f, 0.20f, true,  nullptr },
			{ TEXT("Adrenaline"),       TEXT("아드레날린"),      0.14f,  0.20f,  0.f, 0.f,     true,  nullptr },
			{ TEXT("KeenBlunt"),        TEXT("예리한 둔기"),     0.44f,  0.52f,  0.10f, 0.10f, true,  nullptr },
			{ TEXT("CursedDoll"),       TEXT("저주받은 인형"),    0.14f,  0.17f,  0.25f, 0.25f, true,  nullptr },
			{ TEXT("HitMaster"),        TEXT("타격의 대가"),     0.14f,  0.17f,  0.f, 0.f,     true,  nullptr },
			{ TEXT("AllOutAttack"),     TEXT("속전속결"),       0.18f,  0.21f,  0.20f, 0.20f, true,  nullptr },
			{ TEXT("RaidCaptain"),      TEXT("돌격대장"),       0.40f,  0.48f,  0.f, 0.f,     true,  nullptr },
			{ TEXT("SpiritAbsorption"), TEXT("정기 흡수"),      0.13f,  0.16f,  0.f, 0.f,     true,  nullptr },
			{ TEXT("MagickStream"),     TEXT("마나의 흐름"),     0.07f,  0.10f,  0.02f, 0.02f, true,  nullptr },
			{ TEXT("MasterOfEscape"),   TEXT("탈출의 명수"),     0.23f,  0.27f,  0.f, 0.f,     true,  nullptr },
			{ TEXT("Disrespect"),       TEXT("약자 무시"),      0.33f,  0.33f,  0.30f, 0.40f, true,  nullptr },
			{ TEXT("EtherPredator"),    TEXT("에테르 포식자"),    0.0042f, 0.0054f, 0.01f, 0.01f, true, nullptr },
			{ TEXT("StabilizedStatus"), TEXT("안정된 상태"),     0.14f,  0.17f,  0.f, 0.f,     true,  nullptr },
			{ TEXT("SuperCharge"),      TEXT("슈퍼 차지"),      0.18f,  0.21f,  0.40f, 0.40f, true,  nullptr },
			{ TEXT("StrongWill"),       TEXT("굳은 의지"),      0.28f,  0.32f,  0.f, 0.f,     true,  nullptr },
			{ TEXT("CrisisEvasion"),    TEXT("위기 모면"),      560.f,  480.f,  0.f, 0.f,     true,  nullptr },
			{ TEXT("MaxMPIncrease"),    TEXT("최대 마나 증가"),   0.28f,  0.32f,  0.f, 0.f,     true,  nullptr },
			{ TEXT("MPEfficiency"),     TEXT("마나 효율 증가"),   0.13f,  0.16f,  0.20f, 0.20f, true,  nullptr },
			{ TEXT("Fortitude"),        TEXT("불굴"),          0.28f,  0.32f,  0.f, 0.f,     true,  nullptr },
			{ TEXT("CrushingFist"),     TEXT("분쇄의 주먹"),     0.18f,  0.24f,  0.f, 0.f,     true,  nullptr },
			{ TEXT("MastersTenacity"),  TEXT("달인의 저력"),     0.14f,  0.17f,  0.f, 0.f,     true,  nullptr },
			{ TEXT("DivineProtection"), TEXT("여신의 가호"),     0.22f,  0.30f,  0.f, 0.f,     true,  nullptr },
			{ TEXT("HeavyArmor"),       TEXT("중갑 착용"),      0.90f,  1.06f,  0.f, 0.f,     true,  nullptr },
			{ TEXT("BrokenBone"),       TEXT("부러진 뼈"),      0.37f,  0.42f,  0.f, 0.f,     true,  nullptr },
			{ TEXT("AmbushMaster"),     TEXT("기습의 대가"),     0.048f, 0.076f, 0.15f, 0.15f, true,  nullptr },
			{ TEXT("MasterBrawler"),    TEXT("결투의 대가"),     0.048f, 0.076f, 0.15f, 0.15f, true,  nullptr },
			{ TEXT("MassIncrease"),     TEXT("질량 증가"),      0.16f,  0.19f,  0.10f, 0.10f, true,  nullptr },
			{ TEXT("Propulsion"),       TEXT("추진력"),        0.14f,  0.17f,  0.f, 0.f,     true,  nullptr },
			{ TEXT("EmergencyRescue"),  TEXT("긴급구조"),       200.f,  160.f,  0.f, 0.f,     true,  nullptr },
			{ TEXT("PreciseDagger"),    TEXT("정밀 단도"),      0.18f,  0.21f,  0.06f, 0.06f, true,  nullptr },
			// ── 이 게임에 그 시스템이 없어 효과 없음 (장착·각인서는 됨) ──
			{ TEXT("DropsOfEther"),     TEXT("구슬동자"),       0.f, 0.16f, 0.f, 0.f, false, TEXT("파티·에테르 획득이 없음") },
			{ TEXT("VitalPointHit"),    TEXT("급소 타격"),      0.32f, 0.37f, 0.f, 0.f, false, TEXT("무력화 게이지가 없음") },
			{ TEXT("ShieldPiercing"),   TEXT("실드 관통"),      0.92f, 1.08f, 0.f, 0.f, false, TEXT("보호막을 가진 적이 없음") },
			{ TEXT("ExplosiveExpert"),  TEXT("폭발물 전문가"),   0.80f, 0.80f, 0.f, 0.f, false, TEXT("폭탄 배틀아이템이 없음") },
			{ TEXT("EnhancedShield"),   TEXT("강화 방패"),      0.56f, 0.48f, 0.f, 0.f, false, TEXT("보호막 스킬이 없음") },
			{ TEXT("Necromancy"),       TEXT("강령술"),        17.f, 13.f,   0.f, 0.f, false, TEXT("소환 시스템이 없음") },
			{ TEXT("PreemptiveStrike"), TEXT("선수필승"),       1.40f, 1.70f, 0.f, 0.f, false, TEXT("도전 등급 이하 몬스터 전용 — 보스엔 안 걸림") },
			{ TEXT("LightningFury"),    TEXT("번개의 분노"),     1.00f, 1.40f, 0.f, 0.f, false, TEXT("도전 등급 이하 몬스터 전용") },
			{ TEXT("Contender"),        TEXT("승부사"),        8.f, 12.f,    0.f, 0.f, false, TEXT("처치할 적이 보스뿐") },
			{ TEXT("Barricade"),        TEXT("바리케이드"),      0.14f, 0.17f, 0.f, 0.f, false, TEXT("보호막 스킬이 없음") },
			{ TEXT("Awakening"),        TEXT("각성"),          0.44f, 0.50f, 0.f, 0.f, false, TEXT("각성기가 없음") },
			{ TEXT("SightFocus"),       TEXT("시선 집중"),      0.25f, 0.30f, 0.f, 0.f, false, TEXT("채팅 입력이 없음") },
			{ TEXT("Expert"),           TEXT("전문의"),        0.24f, 0.28f, 0.f, 0.f, false, TEXT("파티 회복·보호막이 없음") },
		};

		const FEngRow* Eng_FindRow(FName Id)
		{
			for (const FEngRow& Row : Eng_Rows)
			{
				if (Id == FName(Row.Id)) return &Row;
			}
			return nullptr;
		}

		float Eng_A(const FEngRow& R, int32 Level) { return FMath::Lerp(R.A0, R.A4, FMath::Clamp(Level, 0, MaxLevel) / static_cast<float>(MaxLevel)); }
		float Eng_B(const FEngRow& R, int32 Level) { return FMath::Lerp(R.B0, R.B4, FMath::Clamp(Level, 0, MaxLevel) / static_cast<float>(MaxLevel)); }

		void Eng_Add(TArray<FStatModifier>& Out, EStatModifierTarget Target, double Value)
		{
			FStatModifier& Mod = Out.AddDefaulted_GetRef();
			Mod.Target = Target;
			Mod.Value = Value;
		}

		FString Eng_P(float V) { return FString::Printf(TEXT("%.2f%%"), V * 100.f); }

		TArray<FEngravingDefinition> BuildDefinitions()
		{
			TArray<FEngravingDefinition> Out;
			for (const FEngRow& Row : Eng_Rows)
			{
				FEngravingDefinition& Def = Out.AddDefaulted_GetRef();
				Def.Id = FName(Row.Id);
				Def.Name = FText::FromString(Row.Name);
				Def.BookItemId = FName(*FString::Printf(TEXT("EngBook_%s"), Row.Id));
				Def.IconName = FString::Printf(TEXT("T_Eng_%s"), Row.Id);
				Def.bHasEffect = Row.bWorks;
			}
			return Out;
		}
	}

	const TArray<FEngravingDefinition>& GetAll()
	{
		static const TArray<FEngravingDefinition> All = BuildDefinitions();
		return All;
	}

	const FEngravingDefinition* Find(FName Id)
	{
		return GetAll().FindByPredicate([Id](const FEngravingDefinition& D) { return D.Id == Id; });
	}

	const FEngravingDefinition* FindByBook(FName BookItemId)
	{
		return GetAll().FindByPredicate([BookItemId](const FEngravingDefinition& D) { return D.BookItemId == BookItemId; });
	}

	int32 LevelFromBooks(int32 BooksRead)
	{
		return FMath::Clamp(BooksRead / BooksPerLevel, 0, MaxLevel);
	}

	float GetValueA(FName Id, int32 Level)
	{
		const FEngRow* R = Eng_FindRow(Id);
		return R ? Eng_A(*R, Level) : 0.f;
	}

	void AppendStaticModifiers(FName Id, int32 Level, TArray<FStatModifier>& Out)
	{
		const FEngRow* R = Eng_FindRow(Id);
		if (!R || !R->bWorks) return;
		const float A = Eng_A(*R, Level);
		const float B = Eng_B(*R, Level);
		const FString S = R->Id;
		using T = EStatModifierTarget;

		// 이 게임의 적은 보스·허수아비뿐이라 "보스 대상" 효과는 전부에 건다
		if (S == TEXT("Grudge"))                { Eng_Add(Out, T::MultiplicativeDamage, A); Eng_Add(Out, T::IncomingDamage, B); }
		else if (S == TEXT("HitMaster"))        { Eng_Add(Out, T::MultiplicativeDamage, A); }  // 소서리스는 타대 — 전 스킬
		else if (S == TEXT("CursedDoll"))       { Eng_Add(Out, T::MultiplicativeDamage, A); Eng_Add(Out, T::HealingReceived, -B); }
		else if (S == TEXT("KeenBlunt"))        { Eng_Add(Out, T::CritDamage, A); Eng_Add(Out, T::DamagePenaltyChance, B); }
		else if (S == TEXT("AllOutAttack"))     { Eng_Add(Out, T::CastSkillDamage, A); Eng_Add(Out, T::CastSpeed, B); }
		else if (S == TEXT("RaidCaptain"))      { Eng_Add(Out, T::MoveSpeedToDamage, A); }
		else if (S == TEXT("SpiritAbsorption")) { Eng_Add(Out, T::SpeedBonus, A); }
		else if (S == TEXT("MasterOfEscape"))   { Eng_Add(Out, T::GetUpCooldown, A); }
		else if (S == TEXT("Disrespect"))       { Eng_Add(Out, T::LowTargetHPDamage, A); Eng_Add(Out, T::LowTargetHPThreshold, B); }
		else if (S == TEXT("StabilizedStatus")) { Eng_Add(Out, T::HighSelfHPDamage, A); }
		else if (S == TEXT("SuperCharge"))      { Eng_Add(Out, T::ChargeSkillDamage, A); Eng_Add(Out, T::ChargeSpeed, B); }
		else if (S == TEXT("StrongWill"))       { Eng_Add(Out, T::PushedDamageReduction, A); }
		else if (S == TEXT("CrisisEvasion"))    { Eng_Add(Out, T::CrisisEvasionCooldown, A); }
		else if (S == TEXT("MaxMPIncrease"))    { Eng_Add(Out, T::MaxMPBonus, A); }
		else if (S == TEXT("MPEfficiency"))     { Eng_Add(Out, T::ManaSkillDamage, A); Eng_Add(Out, T::MPRegen, B); }
		else if (S == TEXT("Fortitude"))        { Eng_Add(Out, T::FortitudeMax, A); }
		else if (S == TEXT("MastersTenacity"))  { Eng_Add(Out, T::LowSelfHPDamage, A); }
		else if (S == TEXT("DivineProtection")) { Eng_Add(Out, T::DivineChance, A); }
		else if (S == TEXT("HeavyArmor"))       { Eng_Add(Out, T::DefensePercent, A); }
		else if (S == TEXT("BrokenBone"))       { Eng_Add(Out, T::GroggyTargetDamage, A); }  // 로아 "무력화 상태" = 우리 그로기
		else if (S == TEXT("AmbushMaster") || S == TEXT("MasterBrawler")) { Eng_Add(Out, T::MultiplicativeDamage, A); }  // 백·헤드 추가분은 소서리스가 못 받음
		else if (S == TEXT("MassIncrease"))     { Eng_Add(Out, T::MultiplicativeDamage, A); Eng_Add(Out, T::AttackSpeedBonus, -B); }
		else if (S == TEXT("EmergencyRescue"))  { Eng_Add(Out, T::EmergencyRescueCooldown, A); }
		else if (S == TEXT("PreciseDagger"))    { Eng_Add(Out, T::CritRate, A); Eng_Add(Out, T::CritDamage, -B); }
		// 아드레날린·마나의 흐름·에테르 포식자·분쇄의 주먹·추진력은 중첩/발동 효과 — AppendStackModifiers
	}

	void AppendStackModifiers(FName Id, int32 Level, int32 Stacks, TArray<FStatModifier>& Out)
	{
		const FEngRow* R = Eng_FindRow(Id);
		if (!R || Stacks <= 0) return;
		const float A = Eng_A(*R, Level);
		const float B = Eng_B(*R, Level);
		if (Id == Adrenaline)
		{
			const int32 S = FMath::Min(Stacks, AdrenalineMaxStacks);
			Eng_Add(Out, EStatModifierTarget::AttackPowerPercent, AdrenalineAttackPerStack * S);
			if (S >= AdrenalineMaxStacks) Eng_Add(Out, EStatModifierTarget::CritRate, A);
		}
		else if (Id == MagickStream)
		{
			const int32 S = FMath::Min(Stacks, MagickStreamMaxStacks);
			Eng_Add(Out, EStatModifierTarget::MPRegen, B * S);
			if (S >= MagickStreamMaxStacks) Eng_Add(Out, EStatModifierTarget::CooldownReduction, A);
		}
		else if (Id == EtherPredator)
		{
			const int32 S = FMath::Min(Stacks, EtherMaxStacks);
			Eng_Add(Out, EStatModifierTarget::AttackPowerPercent, A * S);
			Eng_Add(Out, EStatModifierTarget::DefensePercent, B * S);
		}
		else if (Id == CrushingFist)
		{
			Eng_Add(Out, EStatModifierTarget::AttackPowerPercent, A);
		}
		else if (Id == Propulsion)
		{
			Eng_Add(Out, EStatModifierTarget::SkillDamage, A);
		}
	}

	FString Describe(FName Id, int32 Level)
	{
		TArray<TPair<FString, FString>> Lines;
		DescribeLines(Id, Level, Lines);
		FString Out;
		for (const TPair<FString, FString>& Line : Lines)
		{
			if (!Out.IsEmpty()) Out += TEXT(", ");
			Out += Line.Value.IsEmpty() ? Line.Key : FString::Printf(TEXT("%s %s"), *Line.Key, *Line.Value);
		}
		return Out;
	}

	void DescribeLines(FName Id, int32 Level, TArray<TPair<FString, FString>>& Out)
	{
		const FEngRow* R = Eng_FindRow(Id);
		if (!R) return;
		const float A = Eng_A(*R, Level);
		const float B = Eng_B(*R, Level);
		const FString S = R->Id;
		auto Add = [&Out](const FString& Label, const FString& Value) { Out.Emplace(Label, Value); };

		if (S == TEXT("Grudge"))                { Add(TEXT("보스 몬스터 피해 증가"), Eng_P(A)); Add(TEXT("받는 피해 증가"), Eng_P(B)); }
		else if (S == TEXT("Adrenaline"))       { Add(TEXT("스킬 사용 시 6초간 공격력 (최대 6중첩)"), Eng_P(AdrenalineAttackPerStack)); Add(TEXT("최대 중첩 시 치명타 적중률 증가"), Eng_P(A)); }
		else if (S == TEXT("KeenBlunt"))        { Add(TEXT("치명타 피해 증가"), Eng_P(A)); Add(TEXT("공격 시 피해 -20% 발동 확률"), Eng_P(B)); }
		else if (S == TEXT("CursedDoll"))       { Add(TEXT("적에게 주는 피해 증가"), Eng_P(A)); Add(TEXT("받는 회복 효과 감소"), Eng_P(B)); }
		else if (S == TEXT("HitMaster"))        { Add(TEXT("백·헤드어택이 아닌 공격 피해 증가"), Eng_P(A)); }
		else if (S == TEXT("AllOutAttack"))     { Add(TEXT("캐스팅·홀딩 스킬 피해 증가"), Eng_P(A)); Add(TEXT("캐스팅·홀딩 속도 증가"), Eng_P(B)); }
		else if (S == TEXT("RaidCaptain"))      { Add(TEXT("이동속도 증가량의 피해량 전환 비율"), Eng_P(A)); }
		else if (S == TEXT("SpiritAbsorption")) { Add(TEXT("공격·이동 속도 증가"), Eng_P(A)); }
		else if (S == TEXT("MagickStream"))     { Add(TEXT("스킬 사용 시 10초간 마나 회복 (최대 10중첩)"), Eng_P(B)); Add(TEXT("최대 중첩 시 재사용 대기시간 감소"), Eng_P(A)); }
		else if (S == TEXT("MasterOfEscape"))   { Add(TEXT("기상 재사용 대기시간 감소"), Eng_P(A)); }
		else if (S == TEXT("Disrespect"))       { Add(FString::Printf(TEXT("HP %.0f%% 이하 적에게 주는 피해 증가"), B * 100.f), Eng_P(A)); }
		else if (S == TEXT("EtherPredator"))    { Add(TEXT("스킬 사용 시 90초간 공격력 (최대 30중첩, 4초에 한 번)"), Eng_P(A)); Add(TEXT("중첩당 방어력 증가"), Eng_P(B)); }
		else if (S == TEXT("StabilizedStatus")) { Add(TEXT("HP 65% 이상일 때 주는 피해 증가"), Eng_P(A)); }
		else if (S == TEXT("SuperCharge"))      { Add(TEXT("차지 스킬 피해 증가"), Eng_P(A)); Add(TEXT("차지 속도 증가"), Eng_P(B)); }
		else if (S == TEXT("StrongWill"))       { Add(TEXT("밀려날 때(넉다운·끌려감) 받는 피해 감소"), Eng_P(A)); }
		else if (S == TEXT("CrisisEvasion"))    { Add(TEXT("치명적인 피해를 받으면 3초간 무적, 그동안 받은 피해의 40% 회복"), TEXT("")); Add(TEXT("재사용 대기시간"), FString::Printf(TEXT("%.0f초"), A)); }
		else if (S == TEXT("MaxMPIncrease"))    { Add(TEXT("최대 마나 증가"), Eng_P(A)); }
		else if (S == TEXT("MPEfficiency"))     { Add(TEXT("마나 소모 스킬 피해 증가"), Eng_P(A)); Add(TEXT("마나 회복 증가"), Eng_P(B)); }
		else if (S == TEXT("Fortitude"))        { Add(TEXT("잃은 HP에 비례해 받는 피해 감소 (최대)"), Eng_P(A)); }
		else if (S == TEXT("CrushingFist"))     { Add(TEXT("카운터 성공 시 3초간 공격력 증가"), Eng_P(A)); }
		else if (S == TEXT("MastersTenacity"))  { Add(TEXT("HP 50% 이하일 때 주는 피해 증가"), Eng_P(A)); }
		else if (S == TEXT("DivineProtection")) { Add(TEXT("피격 시 받는 피해 -46% 발동 확률 (10초에 한 번)"), Eng_P(A)); }
		else if (S == TEXT("HeavyArmor"))       { Add(TEXT("물리·마법 방어력 증가"), Eng_P(A)); }
		else if (S == TEXT("BrokenBone"))       { Add(TEXT("무력화(그로기) 상태 적에게 주는 피해 증가"), Eng_P(A)); }
		else if (S == TEXT("AmbushMaster"))     { Add(TEXT("적에게 주는 피해 증가"), Eng_P(A)); Add(TEXT("백어택 피해 증가 (소서리스 해당 없음)"), Eng_P(B)); }
		else if (S == TEXT("MasterBrawler"))    { Add(TEXT("적에게 주는 피해 증가"), Eng_P(A)); Add(TEXT("헤드어택 피해 증가 (소서리스 해당 없음)"), Eng_P(B)); }
		else if (S == TEXT("MassIncrease"))     { Add(TEXT("적에게 주는 피해 증가"), Eng_P(A)); Add(TEXT("공격 속도 감소"), Eng_P(B)); }
		else if (S == TEXT("Propulsion"))       { Add(TEXT("이동기(대시) 사용 후 5초간 스킬 피해 증가 (기본공격 제외)"), Eng_P(A)); }
		else if (S == TEXT("EmergencyRescue"))  { Add(TEXT("HP 30% 미만이 되면 6초간 최대 HP 50% 보호막, 남은 보호막의 50% 회복"), TEXT("")); Add(TEXT("재사용 대기시간"), FString::Printf(TEXT("%.0f초"), A)); }
		else if (S == TEXT("PreciseDagger"))    { Add(TEXT("치명타 적중률 증가"), Eng_P(A)); Add(TEXT("치명타 피해 감소"), Eng_P(B)); }
		// 효과 없는 각인 — 로아 원래 효과 + 이유
		else if (S == TEXT("DropsOfEther"))     { Add(TEXT("공격 시 일정 확률로 파티용 에테르 생성"), TEXT("")); }
		else if (S == TEXT("VitalPointHit"))    { Add(TEXT("무력화 공격 효과 증가"), Eng_P(A)); }
		else if (S == TEXT("ShieldPiercing"))   { Add(TEXT("보호막에 주는 피해 증가"), Eng_P(A)); }
		else if (S == TEXT("ExplosiveExpert"))  { Add(TEXT("폭탄·수류탄 피해 증가"), Eng_P(A)); }
		else if (S == TEXT("EnhancedShield"))   { Add(TEXT("보호막 중 상태이상 면역, 보호막량 감소"), Eng_P(A)); }
		else if (S == TEXT("Necromancy"))       { Add(TEXT("공격 시 병사 소환 (재사용 대기시간)"), FString::Printf(TEXT("%.0f초"), A)); }
		else if (S == TEXT("PreemptiveStrike")) { Add(TEXT("HP가 가득 찬 도전 등급 이하 몬스터에게 치명타 피해 배율"), Eng_P(A)); }
		else if (S == TEXT("LightningFury"))    { Add(TEXT("번개 구슬 5개가 터져 도전 등급 이하 몬스터 추가 피해"), Eng_P(A)); }
		else if (S == TEXT("Contender"))        { Add(TEXT("적 처치 시 40초간 공격력 +2.1% (최대 중첩)"), FString::Printf(TEXT("%.0f"), A)); }
		else if (S == TEXT("Barricade"))        { Add(TEXT("보호막이 있을 때 주는 피해 증가"), Eng_P(A)); }
		else if (S == TEXT("Awakening"))        { Add(TEXT("각성기 재사용 대기시간 감소"), Eng_P(A)); }
		else if (S == TEXT("SightFocus"))       { Add(TEXT("채팅에 \"!!!!!\" 입력 시 6초간 스킬 피해 증가"), Eng_P(A)); }
		else if (S == TEXT("Expert"))           { Add(TEXT("파티원 보호막·회복 효과 증가"), Eng_P(A)); }

		if (!R->bWorks && R->NoEffectReason)
		{
			Add(FString::Printf(TEXT("※ 효과 없음 — %s"), R->NoEffectReason), TEXT(""));
		}
	}

	UTexture2D* LoadIcon(FName Id)
	{
		const FEngravingDefinition* Def = Find(Id);
		if (!Def) return nullptr;
		const FString Path = FString::Printf(TEXT("/Game/LostArk/UI/Engraving/%s.%s"), *Def->IconName, *Def->IconName);
		return TSoftObjectPtr<UTexture2D>(FSoftObjectPath(Path)).LoadSynchronous();
	}

	void RegisterItems(TMap<FName, FItemDefinition>& Map)
	{
		int32 Order = 0;
		for (const FEngravingDefinition& Eng : GetAll())
		{
			FItemDefinition Def;
			Def.ItemId = Eng.BookItemId;
			Def.DisplayName = FText::FromString(FString::Printf(TEXT("유물 %s 각인서"), *Eng.Name.ToString()));
			Def.Description = FText::FromString(FString::Printf(TEXT("%s\n\n우클릭으로 읽기 — %d권마다 +1단계 (최대 4단계, 20권)"),
				*Describe(Eng.Id, 0), BooksPerLevel));
			Def.Category = EItemCategory::Book;
			Def.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LostArk/UI/Engraving/T_EngravingBook.T_EngravingBook")));
			Def.MaxStack = 999;
			Def.SortOrder = Order++;
			Def.SellPrice = 500;
			Map.Add(Def.ItemId, Def);
		}
	}
}
