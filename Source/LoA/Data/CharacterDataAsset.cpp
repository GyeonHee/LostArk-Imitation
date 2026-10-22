#include "Data/CharacterDataAsset.h"

UCharacterDataAsset::UCharacterDataAsset()
{
	// 시작 치명 280 = 치명률 10.024% — AttackPowerScale이 이 기대값에 맞춰 역산돼 있다
	// 6단계 — 시작 특성은 시작 장신구(치명 750 + 특화 750)가 준다. 예전 DA 치명 280은 0으로
	BaseCombatStats.Crit = 0;
}
