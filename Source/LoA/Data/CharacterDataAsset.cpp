#include "Data/CharacterDataAsset.h"

UCharacterDataAsset::UCharacterDataAsset()
{
	// 시작 치명 280 = 치명률 10.024% — AttackPowerScale이 이 기대값에 맞춰 역산돼 있다
	BaseCombatStats.Crit = 280;
}
