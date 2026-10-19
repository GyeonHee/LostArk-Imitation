#include "Item/ItemTypes.h"
#include "Engine/Texture2D.h"
#include "NiagaraSystem.h"

namespace LoAItems
{
	const FName SpiritRecoveryPotion(TEXT("SpiritRecoveryPotion"));

	namespace
	{
		TMap<FName, FItemDefinition> BuildRegistry()
		{
			TMap<FName, FItemDefinition> Map;

			FItemDefinition Potion;
			Potion.ItemId = SpiritRecoveryPotion;
			Potion.DisplayName = NSLOCTEXT("Item", "SpiritRecoveryPotion", "정령의 회복약");
			Potion.Category = EItemCategory::BattleItem;
			Potion.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/LostArk/UI/Item/T_SpiritRecoveryPotion.T_SpiritRecoveryPotion")));
			Potion.MaxStack = 999;
			Potion.SellPrice = 1;
			Potion.bIsBattleItem = true;
			Potion.BattleEquipMax = 7;
			Potion.HealMaxHPRatio = 0.6f;
			Potion.Cooldown = 10.f;
			Potion.UseVFX = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Free_Magic/VFX_Niagara/NS_Heal.NS_Heal")));
			Potion.UseVFXDuration = 1.5f;
			Map.Add(Potion.ItemId, Potion);

			return Map;
		}
	}

	FString FormatWithCommas(int64 Value)
	{
		const bool bNegative = Value < 0;
		const FString Digits = FString::Printf(TEXT("%lld"), bNegative ? -Value : Value);
		FString Out;
		const int32 Len = Digits.Len();
		for (int32 i = 0; i < Len; ++i)
		{
			if (i > 0 && (Len - i) % 3 == 0) Out.AppendChar(TEXT(','));
			Out.AppendChar(Digits[i]);
		}
		return bNegative ? TEXT("-") + Out : Out;
	}

	const FItemDefinition* Find(FName ItemId)
	{
		// 해시맵 — 이름으로 O(1). 처음 부를 때 한 번만 만든다
		static const TMap<FName, FItemDefinition> Registry = BuildRegistry();
		return ItemId.IsNone() ? nullptr : Registry.Find(ItemId);
	}
}
