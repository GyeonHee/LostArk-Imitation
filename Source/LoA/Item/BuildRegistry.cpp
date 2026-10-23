#include "Item/BuildTypes.h"
#include "Item/ItemTypes.h"
#include "Engine/Texture2D.h"

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

	void RegisterItems(TMap<FName, FItemDefinition>& Map)
	{
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

	namespace
	{
		const FName Grudge(TEXT("Grudge"));
		const FName HitMaster(TEXT("HitMaster"));
		const FName CursedDoll(TEXT("CursedDoll"));
		const FName KeenBlunt(TEXT("KeenBlunt"));
		const FName AllOutAttack(TEXT("AllOutAttack"));
		const FName RaidCaptain(TEXT("RaidCaptain"));
		const FName SpiritAbsorption(TEXT("SpiritAbsorption"));
		const FName MasterOfEscape(TEXT("MasterOfEscape"));

		// 유물 0~4단계 (Maxroll 게임 데이터 engraving 1xxx, 어빌리티 스톤 0)
		const float Eng_Grudge[5]       = { 0.18f, 0.1875f, 0.195f, 0.2025f, 0.21f };
		const float Eng_HitMaster[5]    = { 0.14f, 0.1475f, 0.155f, 0.1625f, 0.17f };
		const float Eng_CursedDoll[5]   = { 0.14f, 0.1475f, 0.155f, 0.1625f, 0.17f };
		const float Eng_KeenBlunt[5]    = { 0.44f, 0.46f, 0.48f, 0.50f, 0.52f };
		const float Eng_AdrenCrit[5]    = { 0.14f, 0.155f, 0.17f, 0.185f, 0.20f };
		const float Eng_AllOut[5]       = { 0.18f, 0.1875f, 0.195f, 0.2025f, 0.21f };
		const float Eng_RaidCaptain[5]  = { 0.40f, 0.42f, 0.44f, 0.46f, 0.48f };
		const float Eng_Spirit[5]       = { 0.13f, 0.1375f, 0.145f, 0.1525f, 0.16f };
		const float Eng_MagickCdr[5]    = { 0.07f, 0.0775f, 0.085f, 0.0925f, 0.10f };
		const float Eng_Escape[5]       = { 0.23f, 0.24f, 0.25f, 0.26f, 0.27f };

		constexpr float Eng_GrudgeIncoming = 0.20f;
		constexpr float Eng_CursedDollHeal = -0.25f;
		constexpr float Eng_KeenBluntChance = 0.10f;
		constexpr float Eng_AllOutSpeed = 0.20f;

		TArray<FEngravingDefinition> BuildDefinitions()
		{
			struct FRow { FName Id; const TCHAR* Name; const TCHAR* Icon; };
			const FRow Rows[] = {
				{ Grudge,           TEXT("원한"),         TEXT("T_Eng_Grudge") },
				{ Adrenaline,       TEXT("아드레날린"),     TEXT("T_Eng_Adrenaline") },
				{ KeenBlunt,        TEXT("예리한 둔기"),    TEXT("T_Eng_KeenBlunt") },
				{ CursedDoll,       TEXT("저주받은 인형"),   TEXT("T_Eng_CursedDoll") },
				{ HitMaster,        TEXT("타격의 대가"),    TEXT("T_Eng_HitMaster") },
				{ AllOutAttack,     TEXT("속전속결"),      TEXT("T_Eng_AllOutAttack") },
				{ RaidCaptain,      TEXT("돌격대장"),      TEXT("T_Eng_RaidCaptain") },
				{ SpiritAbsorption, TEXT("정기 흡수"),     TEXT("T_Eng_SpiritAbsorption") },
				{ MagickStream,     TEXT("마나의 흐름"),    TEXT("T_Eng_MagickStream") },
				{ MasterOfEscape,   TEXT("탈출의 명수"),    TEXT("T_Eng_MasterOfEscape") },
			};
			TArray<FEngravingDefinition> Out;
			for (const FRow& Row : Rows)
			{
				FEngravingDefinition& Def = Out.AddDefaulted_GetRef();
				Def.Id = Row.Id;
				Def.Name = FText::FromString(Row.Name);
				Def.BookItemId = FName(*FString::Printf(TEXT("EngBook_%s"), *Row.Id.ToString()));
				Def.IconName = Row.Icon;
			}
			return Out;
		}

		float Eng_At(const float (&Table)[5], int32 Level) { return Table[FMath::Clamp(Level, 0, MaxLevel)]; }

		void Eng_Add(TArray<FStatModifier>& Out, EStatModifierTarget Target, double Value)
		{
			FStatModifier& Mod = Out.AddDefaulted_GetRef();
			Mod.Target = Target;
			Mod.Value = Value;
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
		if (BooksRead <= 0) return -1;
		return FMath::Clamp((BooksRead - 1) / BooksPerLevel, 0, MaxLevel);
	}

	void AppendStaticModifiers(FName Id, int32 Level, TArray<FStatModifier>& Out)
	{
		if (Id == Grudge)
		{
			// 보스 피해 — 이 게임의 적은 보스·허수아비뿐이라 전부에 건다
			Eng_Add(Out, EStatModifierTarget::MultiplicativeDamage, Eng_At(Eng_Grudge, Level));
			Eng_Add(Out, EStatModifierTarget::IncomingDamage, Eng_GrudgeIncoming);
		}
		else if (Id == HitMaster)
		{
			// 백·헤드어택이 아닌 공격 — 소서리스는 타대 클래스라 전 스킬
			Eng_Add(Out, EStatModifierTarget::MultiplicativeDamage, Eng_At(Eng_HitMaster, Level));
		}
		else if (Id == CursedDoll)
		{
			Eng_Add(Out, EStatModifierTarget::MultiplicativeDamage, Eng_At(Eng_CursedDoll, Level));
			Eng_Add(Out, EStatModifierTarget::HealingReceived, Eng_CursedDollHeal);
		}
		else if (Id == KeenBlunt)
		{
			Eng_Add(Out, EStatModifierTarget::CritDamage, Eng_At(Eng_KeenBlunt, Level));
			Eng_Add(Out, EStatModifierTarget::DamagePenaltyChance, Eng_KeenBluntChance);
		}
		else if (Id == AllOutAttack)
		{
			Eng_Add(Out, EStatModifierTarget::CastSpeed, Eng_AllOutSpeed);
			Eng_Add(Out, EStatModifierTarget::CastSkillDamage, Eng_At(Eng_AllOut, Level));
		}
		else if (Id == RaidCaptain)
		{
			Eng_Add(Out, EStatModifierTarget::MoveSpeedToDamage, Eng_At(Eng_RaidCaptain, Level));
		}
		else if (Id == SpiritAbsorption)
		{
			Eng_Add(Out, EStatModifierTarget::SpeedBonus, Eng_At(Eng_Spirit, Level));
		}
		else if (Id == MasterOfEscape)
		{
			Eng_Add(Out, EStatModifierTarget::GetUpCooldown, Eng_At(Eng_Escape, Level));
		}
	}

	void AppendStackModifiers(FName Id, int32 Level, int32 Stacks, TArray<FStatModifier>& Out)
	{
		if (Stacks <= 0) return;
		if (Id == Adrenaline)
		{
			const int32 S = FMath::Min(Stacks, AdrenalineMaxStacks);
			Eng_Add(Out, EStatModifierTarget::AttackPowerPercent, AdrenalineAttackPerStack * S);
			if (S >= AdrenalineMaxStacks) Eng_Add(Out, EStatModifierTarget::CritRate, Eng_At(Eng_AdrenCrit, Level));
		}
		else if (Id == MagickStream)
		{
			const int32 S = FMath::Min(Stacks, MagickStreamMaxStacks);
			Eng_Add(Out, EStatModifierTarget::MPRegen, MagickStreamMPRegenPerStack * S);
			if (S >= MagickStreamMaxStacks) Eng_Add(Out, EStatModifierTarget::CooldownReduction, Eng_At(Eng_MagickCdr, Level));
		}
	}

	FString Describe(FName Id, int32 Level)
	{
		auto P = [](float V) { return FString::Printf(TEXT("%.2f%%"), V * 100.f); };
		if (Id == Grudge)           return FString::Printf(TEXT("보스 등급 이상 몬스터에게 주는 피해 +%s, 받는 피해 +20%%"), *P(Eng_At(Eng_Grudge, Level)));
		if (Id == HitMaster)        return FString::Printf(TEXT("백·헤드어택이 아닌 공격의 피해 +%s"), *P(Eng_At(Eng_HitMaster, Level)));
		if (Id == CursedDoll)       return FString::Printf(TEXT("공격력이 아닌 주는 피해 +%s, 받는 회복 -25%%"), *P(Eng_At(Eng_CursedDoll, Level)));
		if (Id == KeenBlunt)        return FString::Printf(TEXT("치명타 피해 +%s, 공격 시 10%% 확률로 피해 -20%%"), *P(Eng_At(Eng_KeenBlunt, Level)));
		if (Id == Adrenaline)       return FString::Printf(TEXT("스킬 사용 후 6초간 공격력 +0.90%% (최대 6중첩), 최대 중첩 시 치명타 적중률 +%s"), *P(Eng_At(Eng_AdrenCrit, Level)));
		if (Id == AllOutAttack)     return FString::Printf(TEXT("캐스팅·홀딩 스킬 속도 +20%%, 피해 +%s"), *P(Eng_At(Eng_AllOut, Level)));
		if (Id == RaidCaptain)      return FString::Printf(TEXT("이동속도 증가량의 %s만큼 주는 피해 증가"), *P(Eng_At(Eng_RaidCaptain, Level)));
		if (Id == SpiritAbsorption) return FString::Printf(TEXT("공격·이동 속도 +%s"), *P(Eng_At(Eng_Spirit, Level)));
		if (Id == MagickStream)     return FString::Printf(TEXT("스킬 사용 후 10초간 마나 회복 +2%% (최대 10중첩), 최대 중첩 시 쿨타임 -%s"), *P(Eng_At(Eng_MagickCdr, Level)));
		if (Id == MasterOfEscape)   return FString::Printf(TEXT("기상 재사용 대기시간 -%s"), *P(Eng_At(Eng_Escape, Level)));
		return FString();
	}

	void DescribeLines(FName Id, int32 Level, TArray<TPair<FString, FString>>& Out)
	{
		auto P = [](float V) { return FString::Printf(TEXT("%.2f%%"), V * 100.f); };
		auto Add = [&Out](const TCHAR* Label, const FString& Value) { Out.Emplace(Label, Value); };
		if (Id == Grudge)           { Add(TEXT("보스 몬스터 피해 증가"), P(Eng_At(Eng_Grudge, Level))); Add(TEXT("받는 피해 증가"), P(Eng_GrudgeIncoming)); }
		else if (Id == HitMaster)   { Add(TEXT("백·헤드어택이 아닌 공격 피해 증가"), P(Eng_At(Eng_HitMaster, Level))); }
		else if (Id == CursedDoll)  { Add(TEXT("적에게 주는 피해 증가"), P(Eng_At(Eng_CursedDoll, Level))); Add(TEXT("받는 회복 효과 감소"), P(-Eng_CursedDollHeal)); }
		else if (Id == KeenBlunt)   { Add(TEXT("치명타 피해 증가"), P(Eng_At(Eng_KeenBlunt, Level))); Add(TEXT("피해 감소 발동 확률"), P(Eng_KeenBluntChance)); }
		else if (Id == Adrenaline)  { Add(TEXT("중첩당 공격력 증가"), P(AdrenalineAttackPerStack)); Add(TEXT("치명타 적중률 증가"), P(Eng_At(Eng_AdrenCrit, Level))); }
		else if (Id == AllOutAttack){ Add(TEXT("캐스팅·홀딩 속도 증가"), P(Eng_AllOutSpeed)); Add(TEXT("캐스팅·홀딩 스킬 피해 증가"), P(Eng_At(Eng_AllOut, Level))); }
		else if (Id == RaidCaptain) { Add(TEXT("이동속도 증가량의 피해량 전환 비율"), P(Eng_At(Eng_RaidCaptain, Level))); }
		else if (Id == SpiritAbsorption) { Add(TEXT("공격·이동 속도 증가"), P(Eng_At(Eng_Spirit, Level))); }
		else if (Id == MagickStream){ Add(TEXT("중첩당 마나 회복 증가"), P(MagickStreamMPRegenPerStack)); Add(TEXT("최대 중첩 시 재사용 대기시간 감소"), P(Eng_At(Eng_MagickCdr, Level))); }
		else if (Id == MasterOfEscape) { Add(TEXT("기상 재사용 대기시간 감소"), P(Eng_At(Eng_Escape, Level))); }
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
			Def.Description = FText::FromString(FString::Printf(TEXT("%s\n\n우클릭으로 읽기 — 1권이면 배움(유물 0단계), 이후 %d권마다 +1단계 (최대 4)"),
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
