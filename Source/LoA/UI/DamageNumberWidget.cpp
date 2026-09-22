#include "DamageNumberWidget.h"
#include "Components/TextBlock.h"

void UDamageNumberWidget::SetDamage(float Damage)
{
	if (!DamageText) return;

	// 데미지가 수만 단위라 자릿수 구분이 없으면 읽기 어렵다 (보스 HP 표기와 같은 방침)
	DamageText->SetText(FText::FromString(FString::FormatAsNumber(FMath::RoundToInt(Damage))));
}
